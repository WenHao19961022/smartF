#ifndef MQTT_MESSAGE_SENDER_API_H
#define MQTT_MESSAGE_SENDER_API_H

#include <cstdint> // 包含cstdint头文件以使用固定宽度整数类型
#include <../../cv_model/api/cv_model_api.h> // 包含cv_model_api.h头文件以使用其中定义的结构体和函数声明
#include <../../stm32_message_recever/api/stm32_message_recever_api.h> // 包含stm32_api.h头文件以使用其中定义的结构体和函数声明

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

#endif // MQTT_MESSAGE_SENDER_API_H