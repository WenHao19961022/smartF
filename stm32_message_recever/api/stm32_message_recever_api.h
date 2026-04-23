#ifndef STM32_MESSAGE_RECEVER_API_H
#define STM32_MESSAGE_RECEVER_API_H

#include <cstdint> // 包含cstdint头文件以使用固定宽度整数类型

struct FrigeratorInfo
{
    uint8_t temperature;
    uint8_t humidity;
    uint8_t weight;
    uint8_t doorStatus; // 0表示门关闭，1表示门打开
};

FrigeratorInfo GetFrigeratorInfo(); // 获取冰箱信息的函数，core可以调用该函数获取最新的冰箱状态信息

#endif // STM32_MESSAGE_RECEVER_API_H