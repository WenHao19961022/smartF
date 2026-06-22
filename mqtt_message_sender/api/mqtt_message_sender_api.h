#ifndef MQTT_MESSAGE_SENDER_API_H
#define MQTT_MESSAGE_SENDER_API_H

#include <cstdint>
#include <string>
#include "cv_model_api.h"
#include "stm32_message_recever_api.h"

// ==================== 结构体 ====================
enum class MqttEventType : uint8_t {
    Snapshot = 0,
    Startup = 1
};

struct MqttFruitItem {
    uint16_t id;
    FruitType type;
    FreshnessLevel freshness;
    uint32_t weight;
    uint32_t putInTime;
    uint8_t locationX;
    uint8_t locationY;
};

struct MqttMessageStruct {
    MqttEventType eventType = MqttEventType::Snapshot;
    uint32_t time;
    uint32_t messageId;
    uint32_t deviceId;
    FrigeratorInfo fridgeInfo;
    uint8_t fruitCount;
    MqttFruitItem fruits[kMaxStaticFruitCount];
};

// ==================== API函数 ====================
bool SendMqttMessage(const MqttMessageStruct& message);
bool IsMqttMessageSenderReady();
void MqttMessageSenderMainLoop();

#endif // MQTT_MESSAGE_SENDER_API_H
