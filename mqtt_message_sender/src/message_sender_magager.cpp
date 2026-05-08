#include <thread>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <iostream>
#include "../include/message_sender_magager.h"
#include <mqtt/async_client.h>
#include "logger.h"

MessageSenderManager& MessageSenderManager::GetInstance() {
    static MessageSenderManager instance;
    return instance;
}

MessageSenderManager::MessageSenderManager() {
    Init();
}

MessageSenderManager::~MessageSenderManager() {
    Disconnect();
}

void MessageSenderManager::Init() {
    LOG_PRINT("[Mqtt]", "=== MessageSenderManager Init START ===");
    mInitStatus.store(kInitUnfinished);
    mSenderIdleStatus.store(kSenderIdle);
    mMessageSendSwitch.store(kMessageSendSwitchOff);
    mConnected.store(false);

    mDataMutex.lock();
    mMessage = {};
    mDataMutex.unlock();

    // 从配置管理器读取MQTT参数
    std::string brokerAddr = ConfigManager::GetInstance().GetString("mqtt.broker_addr", "tcp://101.34.239.30:1883");
    std::string clientId = ConfigManager::GetInstance().GetString("mqtt.client_id", "SmartFridge_MQTT_Client");
    std::string username = ConfigManager::GetInstance().GetString("mqtt.username", "admin");
    std::string password = ConfigManager::GetInstance().GetString("mqtt.password", "admin123");
    int keepalive = ConfigManager::GetInstance().GetInt("mqtt.keepalive", 60);
    std::string topic = ConfigManager::GetInstance().GetString("mqtt.topic", "smartfridge/data");
    int qos = ConfigManager::GetInstance().GetInt("mqtt.qos", 1);

    LOG_PRINT("[Mqtt]", "Config loaded:");
    LOG_PRINT("[Mqtt]", "  - broker_addr: " << brokerAddr);
    LOG_PRINT("[Mqtt]", "  - client_id: " << clientId);
    LOG_PRINT("[Mqtt]", "  - username: " << username);
    LOG_PRINT("[Mqtt]", "  - password: [PROTECTED]");
    LOG_PRINT("[Mqtt]", "  - keepalive: " << keepalive << "s");
    LOG_PRINT("[Mqtt]", "  - topic: " << topic);
    LOG_PRINT("[Mqtt]", "  - qos: " << qos);

    // 尝试连接MQTT服务器
    LOG_PRINT("[Mqtt]", "Attempting to connect to MQTT broker...");
    if (Connect()) {
        LOG_PRINT("[Mqtt]", "Initial connection successful");
        SetReady();
    } else {
        LOG_PRINT("[Mqtt]", "Initial connection failed, will retry on first send");
        SetReady();  // 即使连接失败也标记为ready，后续发送时重连
    }
    LOG_PRINT("[Mqtt]", "=== MessageSenderManager Init END ===");
}

bool MessageSenderManager::Connect() {
    std::lock_guard<std::mutex> lock(mMqttMutex);

    if (mConnected.load()) {
        return true;
    }

    try {
        std::string brokerAddr = ConfigManager::GetInstance().GetString("mqtt.broker_addr", "tcp://101.34.239.30:1883");
        std::string clientId = ConfigManager::GetInstance().GetString("mqtt.client_id", "SmartFridge_MQTT_Client");
        std::string username = ConfigManager::GetInstance().GetString("mqtt.username", "admin");
        std::string password = ConfigManager::GetInstance().GetString("mqtt.password", "admin123");
        mqtt::async_client* client = new mqtt::async_client(brokerAddr, clientId);

        int keepalive = ConfigManager::GetInstance().GetInt("mqtt.keepalive", 60);
        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(std::chrono::seconds(keepalive));
        connOpts.set_clean_session(true);
        connOpts.set_automatic_reconnect(true);
        connOpts.set_user_name(username);
        connOpts.set_password(password);

        // 尝试连接
        mqtt::token_ptr connectToken = client->connect(connOpts);
        connectToken->wait_for(std::chrono::seconds(5));

        if (client->is_connected()) {
            mMqttClient = static_cast<void*>(client);
            mConnected.store(true);
            LOG_PRINT("[Mqtt]", "Successfully connected to broker: " << brokerAddr);
            return true;
        }
    } catch (const mqtt::exception& exc) {
        LOG_PRINT("[Mqtt]", "Connection error: " << exc.what());
    } catch (const std::exception& e) {
        LOG_PRINT("[Mqtt]", "Standard exception during connection: " << e.what());
    }

    return false;
}

bool MessageSenderManager::Disconnect() {
    std::lock_guard<std::mutex> lock(mMqttMutex);

    if (!mConnected.load() || mMqttClient == nullptr) {
        return true;
    }

    try {
        mqtt::async_client* client = static_cast<mqtt::async_client*>(mMqttClient);
        if (client && client->is_connected()) {
            mqtt::token_ptr disconnectToken = client->disconnect();
            disconnectToken->wait_for(std::chrono::seconds(2));
        }
        delete client;
        mMqttClient = nullptr;
        mConnected.store(false);
        LOG_PRINT("[Mqtt]", "Disconnected from broker");
        return true;
    } catch (const mqtt::exception& exc) {
        LOG_PRINT("[Mqtt]", "Disconnect error: " << exc.what());
        return false;
    }
}

void MessageSenderManager::CopyMessage(const MqttMessageStruct& message) {
    std::lock_guard<std::mutex> lock(mDataMutex);
    mMessage = message;
    LOG_PRINT("[Mqtt]", "CopyMessage: deviceId=" << message.deviceId
              << " | msgId=" << message.messageId
              << " | time=" << message.time
              << " | fruitCount=" << (int)message.fruitCount
              << " | temp=" << (message.fridgeInfo.temperature / 10.0) << "C"
              << " | humidity=" << (message.fridgeInfo.humidity / 10.0) << "%"
              << " | weight=" << message.fridgeInfo.weight << "g"
              << " | door=" << (int)message.fridgeInfo.doorStatus);
}

bool MessageSenderManager::SendMessage(const MqttMessageStruct& message) {
    // 如果未连接，尝试重连
    if (!mConnected.load()) {
        LOG_PRINT("[Mqtt]", "Not connected, attempting to reconnect...");
        if (!Connect()) {
            LOG_PRINT("[Mqtt]", "Reconnection failed");
            return false;
        }
    }

    std::lock_guard<std::mutex> lock(mMqttMutex);

    if (mMqttClient == nullptr) {
        LOG_PRINT("[Mqtt]", "Client is null");
        return false;
    }

    try {
        // 序列化为JSON
        std::string jsonPayload = SerializeMqttMessageToJson(message);

        mqtt::async_client* client = static_cast<mqtt::async_client*>(mMqttClient);

        // 从配置管理器获取topic和qos
        std::string topic = ConfigManager::GetInstance().GetString("mqtt.topic", "smartfridge/data");
        int qos = ConfigManager::GetInstance().GetInt("mqtt.qos", 1);

        // 发布消息
        mqtt::message_ptr pubmsg = mqtt::make_message(topic, jsonPayload);
        pubmsg->set_qos(qos);

        mqtt::token_ptr pubToken = client->publish(pubmsg);
        pubToken->wait_for(std::chrono::seconds(3));

        LOG_PRINT("[Mqtt]", "Message published successfully to topic: " << topic);
        LOG_PRINT("[Mqtt]", "Payload size: " << jsonPayload.size() << " bytes");
        LOG_PRINT("[Mqtt]", "Payload JSON:\n" << jsonPayload);
        LOG_PRINT("[Mqtt]", "SendMessage: All " << (int)message.fruitCount << " fruit items published successfully");
        for (uint8_t i = 0; i < message.fruitCount; ++i) {
            LOG_PRINT("[Mqtt]", "  Fruit[" << (int)i << "]: id=" << message.fruits[i].id
                      << " type=" << (int)message.fruits[i].type
                      << " freshness=" << (int)message.fruits[i].freshness
                      << " weight=" << message.fruits[i].weight << "g");
        }
        return true;

    } catch (const mqtt::exception& exc) {
        LOG_PRINT("[Mqtt]", "Publish error: " << exc.what());
        mConnected.store(false);
        return false;
    } catch (const std::exception& e) {
        LOG_PRINT("[Mqtt]", "Standard exception during publish: " << e.what());
        return false;
    }
}

void MessageSenderManager::MainLoop() {
    LOG_PRINT("[Mqtt]", "=== MessageSenderManager MainLoop START ===");
    int loopCount = 0;
    while (true) {
        if (IsMessageSendSwitchOn() && IsIdle()) {
            LOG_PRINT("[Mqtt]", "MainLoop: SendSwitch=ON, SenderIdle=YES -> preparing to send");
            SetSenderStatus(kSenderBusy);

            mDataMutex.lock();
            MqttMessageStruct messageToSend = mMessage;
            LOG_PRINT("[Mqtt]", "MainLoop: Retrieved message | fruitCount=" << (int)messageToSend.fruitCount
                      << " | msgId=" << messageToSend.messageId);
            mDataMutex.unlock();

            uint8_t messageSendCount = 0;
            uint8_t maxMessageSendCount = 3;

            while (true) {
                LOG_PRINT("[Mqtt]", "MainLoop: Send attempt " << (messageSendCount + 1) << "/" << maxMessageSendCount);
                if (SendMessage(messageToSend)) {
                    LOG_PRINT("[Mqtt]", "MainLoop: Send SUCCESS after " << (messageSendCount + 1) << " attempt(s)");
                    break;
                }
                messageSendCount++;
                if (messageSendCount >= maxMessageSendCount) {
                    LOG_PRINT("[Mqtt]", "MainLoop: Send FAILED after " << maxMessageSendCount << " attempts");
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            SetMessageSendSwitch(kMessageSendSwitchOff);
            SetSenderStatus(kSenderIdle);
        }

        loopCount++;
        if (loopCount % 100 == 0) {
            LOG_PRINT("[Mqtt]", "MainLoop: heartbeat | connected=" << mConnected.load()
                      << " | idle=" << IsIdle() << " | switch=" << IsMessageSendSwitchOn());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    LOG_PRINT("[Mqtt]", "=== MessageSenderManager MainLoop END ===");
}

// ========== JSON序列化实现 ==========

std::string FruitTypeToString(FruitType type) {
    // FruitType枚举值直接转换为字符串，与JSON中的type字段对应
    return std::to_string(static_cast<uint8_t>(type));
}

uint8_t FreshnessLevelToScore(FreshnessLevel level) {
    switch (level) {
        case FreshnessLevel::Fresh: return 10;
        case FreshnessLevel::Stale: return 5;
        case FreshnessLevel::Rotten: return 0;
        default: return 8;
    }
}

std::string FormatTimestamp(uint32_t timestamp) {
    time_t rawTime = static_cast<time_t>(timestamp);
    struct tm* timeInfo = localtime(&rawTime);

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (timeInfo->tm_year + 1900) << "-"
        << std::setw(2) << (timeInfo->tm_mon + 1) << "-"
        << std::setw(2) << timeInfo->tm_mday << " "
        << std::setw(2) << timeInfo->tm_hour << ":"
        << std::setw(2) << timeInfo->tm_min << ":"
        << std::setw(2) << timeInfo->tm_sec;

    return oss.str();
}

std::string GenerateDeviceId(uint32_t deviceId) {
    std::ostringstream oss;
    oss << "SN-" << std::setfill('0') << std::setw(3) << deviceId;
    return oss.str();
}

std::string GenerateMessageId(uint32_t messageId) {
    std::ostringstream oss;
    oss << "MSG_" << std::setfill('0') << std::setw(6) << messageId;
    return oss.str();
}

std::string SerializeMqttMessageToJson(const MqttMessageStruct& message) {
    std::ostringstream json;

    json << "{\n";

    // 设备ID
    json << "  \"device_id\": \"" << GenerateDeviceId(message.deviceId) << "\",\n";

    // 消息ID
    json << "  \"message_id\": \"" << GenerateMessageId(message.messageId) << "\",\n";

    // 消息时间
    json << "  \"message_time\": \"" << FormatTimestamp(message.time) << "\",\n";

    // 冰箱信息
    json << "  \"refrigerator_info\": {\n";
    json << "    \"temperature\": " << std::fixed << std::setprecision(1)
         << (message.fridgeInfo.temperature / 10.0) << ",\n";
    json << "    \"humidity\": " << std::fixed << std::setprecision(1)
         << (message.fridgeInfo.humidity / 10.0) << "\n";
    json << "  },\n";

    // 水果数量
    json << "  \"fruit_num\": " << static_cast<int>(message.fruitCount) << ",\n";

    // 水果数组
    json << "  \"fruits\": [\n";
    for (uint8_t i = 0; i < message.fruitCount; ++i) {
        json << "    {\n";
        json << "      \"id\": " << static_cast<int>(message.fruits[i].id) << ",\n";
        json << "      \"type\": \"" << FruitTypeToString(message.fruits[i].type) << "\",\n";
        json << "      \"fresh_status\": " << static_cast<int>(FreshnessLevelToScore(message.fruits[i].freshness)) << ",\n";
        json << "      \"weight\": " << std::fixed << std::setprecision(1)
             << static_cast<double>(message.fruits[i].weight) << "\n";
        json << "    }";

        if (i < message.fruitCount - 1) {
            json << ",";
        }
        json << "\n";
    }

    json << "  ]\n";
    json << "}\n";

    return json.str();
}
