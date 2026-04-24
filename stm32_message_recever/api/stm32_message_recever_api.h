#ifndef STM32_MESSAGE_RECEVER_API_H
#define STM32_MESSAGE_RECEVER_API_H

#include <cstdint> // 包含cstdint头文件以使用固定宽度整数类型

const static uint8_t FRIGERATOR_HISTORY_INFO_SIZE = 5; // 定义一个常量，表示冰箱历史信息的大小

// 门状态枚举
enum class DoorStatus : uint16_t {
    DOOR_CLOSED = 0, // 门关闭
    DOOR_OPEN   = 1, // 门打开
};

struct FrigeratorInfo
{
    uint16_t temperature;
    uint16_t humidity;
    uint16_t weight;
    DoorStatus doorStatus; // 0表示门关闭，1表示门打开
};

struct FrigeratorInfoWithTimestamp
{
    uint32_t timestamp; // 记录时间戳，单位为秒
    FrigeratorInfo info; // 冰箱状态信息
};

struct FrigeratorHistoryInfo
{
    FrigeratorInfoWithTimestamp frigeratorInfoWithTimestamp[FRIGERATOR_HISTORY_INFO_SIZE]; // 存储最近5次的冰箱状态信息,主要用于记录冰箱内水果重量变化(超过一定数量才记录)
};

FrigeratorHistoryInfo GetFrigeratorInfo(); // 获取冰箱信息的函数，core可以调用该函数获取最新的冰箱状态信息

#endif // STM32_MESSAGE_RECEVER_API_H