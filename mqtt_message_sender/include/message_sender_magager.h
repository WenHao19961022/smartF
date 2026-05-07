#ifndef MESSAGE_SENDER_MANAGER_H
#define MESSAGE_SENDER_MANAGER_H

#include <mutex>
#include <atomic>
#include <string>
#include "../api/mqtt_message_sender_api.h"

// MQTT配置常量
const static std::string MQTT_BROKER_ADDR = "tcp://localhost:1883";
const static std::string MQTT_CLIENT_ID = "SmartFridge_MQTT_Client";
const static std::string MQTT_TOPIC = "smartfridge/data";
const static int MQTT_QOS = 1;
const static int MQTT_KEEPALIVE = 60;

const static bool INITI_FINISHED = true;
const static bool INITI_UNFINISHED = false;
const static bool SENDER_IDLE = true;
const static bool SENDER_BUSY = false;
const static bool MESSAGE_SEND_SWITCH_ON = true;
const static bool MESSAGE_SEND_SWITCH_OFF = false;

class MessageSenderManager
{
public:
    static MessageSenderManager& GetInstance();

    void SenderInit();
    bool IsSenderReady() const { return m_initStatus.load(); }
    bool IsMessageSenderIdle() const { return m_SenderIdleStatus.load(); }
    void CopyMessage(const MqttMessageStruct& message);
    void SetMessageSendSwitch(bool flag) { m_messageSendSwitch.store(flag); }

    void MainLoop();

private:
    MessageSenderManager();
    ~MessageSenderManager();
    MessageSenderManager(const MessageSenderManager&) = delete;
    MessageSenderManager& operator=(const MessageSenderManager&) = delete;

    bool Connect();
    bool Disconnect();
    bool SendMessage(const MqttMessageStruct& message);

    void MessageSenderReady() { m_initStatus.store(INITI_FINISHED); }
    bool IsMessageSendSwitchOn() const { return m_messageSendSwitch.load(); }
    void SetMessageSenderStatus(bool idle) { m_SenderIdleStatus.store(idle); }

    std::atomic<bool> m_initStatus{INITI_UNFINISHED};
    std::atomic<bool> m_SenderIdleStatus{SENDER_IDLE};
    std::atomic<bool> m_messageSendSwitch{MESSAGE_SEND_SWITCH_OFF};

    std::mutex m_dataMutex;
    MqttMessageStruct m_message;

    // MQTT客户端相关
    void* m_mqttClient = nullptr;  // MQTTAsync客户端指针
    std::atomic<bool> m_connected{false};
    std::mutex m_mqttMutex;
};

// JSON序列化工具函数声明
std::string SerializeMqttMessageToJson(const MqttMessageStruct& message);

// 辅助函数：将FruitType枚举转换为字符串
std::string FruitTypeToString(FruitType type);

// 辅助函数：将FreshnessLevel转换为评分(0-10)
uint8_t FreshnessLevelToScore(FreshnessLevel level);

// 辅助函数：格式化时间戳为"YYYY-MM-DD HH:MM:SS"
std::string FormatTimestamp(uint32_t timestamp);

// 辅助函数：生成格式化device_id
std::string GenerateDeviceId(uint32_t deviceId);

// 辅助函数：生成格式化message_id
std::string GenerateMessageId(uint32_t messageId);

#endif // MESSAGE_SENDER_MANAGER_H