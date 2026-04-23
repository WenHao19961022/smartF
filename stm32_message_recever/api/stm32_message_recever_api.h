#ifndef STM32_MESSAGE_RECEVER_API_H
#define STM32_MESSAGE_RECEVER_API_H

#include <cstdint> // 包含cstdint头文件以使用固定宽度整数类型

struct FrigeratorInfo
{
    uint16_t temperature;
    uint16_t humidity;
    uint16_t weight;
    uint16_t doorStatus; // 0表示门关闭，1表示门打开
};

struct FrigeratorHistoryInfo
{
    uint32_t timestamp[5]; // 记录时间戳，单位为秒
    FrigeratorInfo historyInfo[5]; // 存储最近5次的冰箱状态信息,主要用于记录冰箱内水果重量变化(超过一定数量才记录)
};

FrigeratorHistoryInfo GetFrigeratorInfo(); // 获取冰箱信息的函数，core可以调用该函数获取最新的冰箱状态信息

#endif // STM32_MESSAGE_RECEVER_API_H