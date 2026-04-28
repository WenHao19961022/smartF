#ifndef MQTT_MESSAGE_SENDER_API_H
#define MQTT_MESSAGE_SENDER_API_H

#include <cstdint> // 包含cstdint头文件以使用固定宽度整数类型
#include "cv_model_api.h" // 使用模块 API 头，不依赖相对路径搜索
#include "stm32_message_recever_api.h" // 使用模块 API 头，不依赖相对路径搜索

struct MqttMessageStruct
{
    uint32_t time;
    uint32_t messageId;
    uint32_t deviceId;
    FrigeratorInfo fridgeInfo;
    uint8_t fruitCount; // 识别到的水果数量
    FruitInfoWithWeight fruits[MAX_STATIC_FRUIT_COUNT]; // 识别到的水果信息数组，假设最多识别10个水果
};

// 服务器连接相关的函数声明
bool SendMqttMessage(const MqttMessageStruct& message); // 发送MQTT消息，core可以调用该函数将消息发送到MQTT服务器

void MqttMessageSenderMainLoop(); // 主循环函数，core可以调用该函数来处理发送MQTT消息的逻辑，例如定时发送消息或根据特定事件触发发送消息

#endif // MQTT_MESSAGE_SENDER_API_H