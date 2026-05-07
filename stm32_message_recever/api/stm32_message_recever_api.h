#ifndef STM32_MESSAGE_RECEVER_API_H
#define STM32_MESSAGE_RECEVER_API_H

#include <cstdint>

// ==================== 常量 ====================
const uint8_t kFridgeHistoryInfoSize = 5;

// ==================== 枚举 ====================
enum class DoorStatus : uint16_t {
    DoorClosed = 0,
    DoorOpen   = 1
};

// ==================== 结构体 ====================
struct FrigeratorInfo {
    uint16_t temperature;
    uint16_t humidity;
    uint16_t weight;
    DoorStatus doorStatus;
};

struct FrigeratorInfoWithTimestamp {
    uint32_t timestamp;
    FrigeratorInfo info;
};

struct FrigeratorHistoryInfo {
    uint32_t temperatureTimestamp[kFridgeHistoryInfoSize];
    uint32_t humidityTimestamp[kFridgeHistoryInfoSize];
    uint32_t weightTimestamp[kFridgeHistoryInfoSize];
    uint32_t doorStatusTimestamp[kFridgeHistoryInfoSize];
    uint16_t temperature[kFridgeHistoryInfoSize];
    uint16_t humidity[kFridgeHistoryInfoSize];
    uint16_t weight[kFridgeHistoryInfoSize];
    DoorStatus doorStatus[kFridgeHistoryInfoSize];
};

// ==================== API函数 ====================
FrigeratorHistoryInfo GetFrigeratorInfo();
void Stm32MessageReceverMainLoop();

#endif // STM32_MESSAGE_RECEVER_API_H
