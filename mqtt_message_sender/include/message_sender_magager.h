#ifndef MESSAGE_SENDER_MANAGER_H
#define MESSAGE_SENDER_MANAGER_H

#include <mutex>
#include <atomic>
#include <string>
#include "../api/mqtt_message_sender_api.h"
#include "config_manager.h"

// ==================== 常量 ====================
const bool kInitFinished = true;
const bool kInitUnfinished = false;
const bool kSenderIdle = true;
const bool kSenderBusy = false;
const bool kMessageSendSwitchOn = true;
const bool kMessageSendSwitchOff = false;

// ==================== 消息发送管理类 ====================
class MessageSenderManager {
public:
    static MessageSenderManager& GetInstance();

    void Init();
    bool IsReady() const { return mInitStatus.load(); }
    bool IsIdle() const { return mSenderIdleStatus.load(); }
    void CopyMessage(const MqttMessageStruct& message);
    void SetMessageSendSwitch(bool flag) { mMessageSendSwitch.store(flag); }
    void MainLoop();

private:
    MessageSenderManager();
    ~MessageSenderManager();

    MessageSenderManager(const MessageSenderManager&) = delete;
    MessageSenderManager& operator=(const MessageSenderManager&) = delete;

    bool Connect();
    bool Disconnect();
    bool SendMessage(const MqttMessageStruct& message);
    void SetReady() { mInitStatus.store(kInitFinished); }
    bool IsMessageSendSwitchOn() const { return mMessageSendSwitch.load(); }
    void SetSenderStatus(bool idle) { mSenderIdleStatus.store(idle); }

    std::atomic<bool> mInitStatus{kInitUnfinished};
    std::atomic<bool> mSenderIdleStatus{kSenderIdle};
    std::atomic<bool> mMessageSendSwitch{kMessageSendSwitchOff};

    std::mutex mDataMutex;
    MqttMessageStruct mMessage;

    void* mMqttClient = nullptr;
    std::atomic<bool> mConnected{false};
    std::mutex mMqttMutex;
};

// ==================== JSON序列化工具函数 ====================
std::string SerializeMqttMessageToJson(const MqttMessageStruct& message);
std::string FruitTypeToString(FruitType type);
uint8_t FreshnessLevelToScore(FreshnessLevel level);
std::string FormatTimestamp(uint32_t timestamp);
std::string GenerateDeviceId(uint32_t deviceId);
std::string GenerateMessageId(uint32_t messageId);

#endif // MESSAGE_SENDER_MANAGER_H