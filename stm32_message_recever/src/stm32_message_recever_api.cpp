#include "../api/stm32_message_recever_api.h"
#include "../include/message_recever_manager.h"
#include "../../common/include/logger.h"

FrigeratorHistoryInfo GetFrigeratorInfo()
{
    // 这里应该包含实际获取冰箱信息的逻辑，例如通过I2C或SPI协议从STM32微控制器读取数据
    // 由于这是一个示例，我们暂时返回一个默认的FrigeratorHistoryInfo对象
    FrigeratorHistoryInfo info = MessageReceverManager::GetInstance().GetFrigeratorHistoryInfo();
    LOG_PRINT("[Stm32]", "GetFrigeratorInfo called | ready="
              << MessageReceverManager::GetInstance().IsReady()
              << " | latest temp=" << (info.temperature[kFridgeHistoryInfoSize - 1]) << "C"
              << " | humidity=" << (info.humidity[kFridgeHistoryInfoSize - 1]) << "%"
              << " | weight=" << info.weight[kFridgeHistoryInfoSize - 1] << "g"
              << " | door=" << (int)info.doorStatus[kFridgeHistoryInfoSize - 1]);
    return info;
}

void Stm32MessageReceverMainLoop()
{
    // 这里应该包含实际处理接收到的消息的逻辑，例如解析消息并更新冰箱状态
    // 由于这是一个示例，我们暂时调用MessageReceverManager的ProcessMessages方法来处理消息
    MessageReceverManager::GetInstance().MainLoop();
}

void StartMockStm32Mode(int doorOpenDurationSec, int doorClosedDurationSec) {
    MessageReceverManager::GetInstance().StartMockMode(doorOpenDurationSec, doorClosedDurationSec);
}

void StopMockStm32Mode() {
    MessageReceverManager::GetInstance().StopMockMode();
}