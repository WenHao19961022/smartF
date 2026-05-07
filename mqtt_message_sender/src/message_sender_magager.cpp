#include <thread>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <iostream>
#include "../include/message_sender_magager.h"
#include <mqtt/async_client.h>

MessageSenderManager& MessageSenderManager::GetInstance()
{
    static MessageSenderManager instance;
    return instance;
}

MessageSenderManager::MessageSenderManager()
{
    SenderInit();
}

MessageSenderManager::~MessageSenderManager()
{
    Disconnect();
}

void MessageSenderManager::SenderInit()
{
    m_initStatus.store(INITI_UNFINISHED);
    m_SenderIdleStatus.store(SENDER_IDLE);
    m_messageSendSwitch.store(MESSAGE_SEND_SWITCH_OFF);
    m_connected.store(false);

    m_dataMutex.lock();
    m_message = {};
    m_dataMutex.unlock();

    // 尝试连接MQTT服务器
    if (Connect()) {
        MessageSenderReady();
    } else {
        MessageSenderReady();  // 即使连接失败也标记为ready，后续发送时重连
    }
}

bool MessageSenderManager::Connect()
{
    std::lock_guard<std::mutex> lock(m_mqttMutex);

    if (m_connected.load()) {
        return true;
    }

    try {
        mqtt::async_client* client = new mqtt::async_client(MQTT_BROKER_ADDR, MQTT_CLIENT_ID);

        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(std::chrono::seconds(MQTT_KEEPALIVE));
        connOpts.set_clean_session(true);
        connOpts.set_automatic_reconnect(true);

        // 尝试连接
        mqtt::token_ptr connectToken = client->connect(connOpts);
        connectToken->wait_for(std::chrono::seconds(5));

        if (client->is_connected()) {
            m_mqttClient = static_cast<void*>(client);
            m_connected.store(true);
            std::cout << "[MQTT] Successfully connected to broker: " << MQTT_BROKER_ADDR << std::endl;
            return true;
        }
    } catch (const mqtt::exception& exc) {
        std::cerr << "[MQTT] Connection error: " << exc.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[MQTT] Standard exception during connection: " << e.what() << std::endl;
    }

    return false;
}

bool MessageSenderManager::Disconnect()
{
    std::lock_guard<std::mutex> lock(m_mqttMutex);

    if (!m_connected.load() || m_mqttClient == nullptr) {
        return true;
    }

    try {
        mqtt::async_client* client = static_cast<mqtt::async_client*>(m_mqttClient);
        if (client && client->is_connected()) {
            mqtt::token_ptr disconnectToken = client->disconnect();
            disconnectToken->wait_for(std::chrono::seconds(2));
        }
        delete client;
        m_mqttClient = nullptr;
        m_connected.store(false);
        std::cout << "[MQTT] Disconnected from broker" << std::endl;
        return true;
    } catch (const mqtt::exception& exc) {
        std::cerr << "[MQTT] Disconnect error: " << exc.what() << std::endl;
        return false;
    }
}

void MessageSenderManager::CopyMessage(const MqttMessageStruct& message)
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_message = message;
}

bool MessageSenderManager::SendMessage(const MqttMessageStruct& message)
{
    // 如果未连接，尝试重连
    if (!m_connected.load()) {
        std::cout << "[MQTT] Not connected, attempting to reconnect..." << std::endl;
        if (!Connect()) {
            std::cerr << "[MQTT] Reconnection failed" << std::endl;
            return false;
        }
    }

    std::lock_guard<std::mutex> lock(m_mqttMutex);

    if (m_mqttClient == nullptr) {
        std::cerr << "[MQTT] Client is null" << std::endl;
        return false;
    }

    try {
        // 序列化为JSON
        std::string jsonPayload = SerializeMqttMessageToJson(message);

        mqtt::async_client* client = static_cast<mqtt::async_client*>(m_mqttClient);

        // 发布消息
        mqtt::message_ptr pubmsg = mqtt::make_message(MQTT_TOPIC, jsonPayload);
        pubmsg->set_qos(MQTT_QOS);

        mqtt::token_ptr pubToken = client->publish(pubmsg);
        pubToken->wait_for(std::chrono::seconds(3));

        std::cout << "[MQTT] Message published successfully to topic: " << MQTT_TOPIC << std::endl;
        std::cout << "[MQTT] Payload size: " << jsonPayload.size() << " bytes" << std::endl;
        return true;

    } catch (const mqtt::exception& exc) {
        std::cerr << "[MQTT] Publish error: " << exc.what() << std::endl;
        m_connected.store(false);
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[MQTT] Standard exception during publish: " << e.what() << std::endl;
        return false;
    }
}

void MessageSenderManager::MainLoop()
{
    while (true)
    {
        if (IsMessageSendSwitchOn() && IsMessageSenderIdle())
        {
            SetMessageSenderStatus(SENDER_BUSY);

            m_dataMutex.lock();
            MqttMessageStruct messageToSend = m_message;
            m_dataMutex.unlock();

            uint8_t messageSendCount = 0;
            uint8_t maxMessageSendCount = 3;

            while (true)
            {
                if (SendMessage(messageToSend))
                {
                    break;
                }
                messageSendCount++;
                if (messageSendCount >= maxMessageSendCount)
                {
                    std::cerr << "[MQTT] Failed to send message after " << maxMessageSendCount << " attempts" << std::endl;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            SetMessageSendSwitch(MESSAGE_SEND_SWITCH_OFF);
            SetMessageSenderStatus(SENDER_IDLE);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ========== JSON序列化实现 ==========

std::string FruitTypeToString(FruitType type) {
    // FruitType枚举值直接转换为字符串，与JSON中的type字段对应
    return std::to_string(static_cast<uint8_t>(type));
}

uint8_t FreshnessLevelToScore(FreshnessLevel level) {
    switch (level) {
        case FreshnessLevel::FRESH: return 10;
        case FreshnessLevel::STALE: return 5;
        case FreshnessLevel::ROTTEN: return 0;
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

    // 添加空的占位水果（如果数量小于MAX_STATIC_FRUIT_COUNT）
    if (message.fruitCount < MAX_STATIC_FRUIT_COUNT) {
        for (uint8_t i = message.fruitCount; i < MAX_STATIC_FRUIT_COUNT; ++i) {
            json << "    {\n";
            json << "      \"id\": \"\",\n";
            json << "      \"type\": \"\",\n";
            json << "      \"fresh_status\": \"\",\n";
            json << "      \"weight\": \"\"\n";
            json << "    }";

            if (i < MAX_STATIC_FRUIT_COUNT - 1) {
                json << ",";
            }
            json << "\n";
        }
    }

    json << "  ]\n";
    json << "}\n";

    return json.str();
}