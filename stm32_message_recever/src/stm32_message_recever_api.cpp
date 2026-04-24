#include "../api/stm32_message_recever_api.h"
#include "../include/message_recever_manager.h"

FrigeratorHistoryInfo GetFrigeratorInfo()
{
    // 这里应该包含实际获取冰箱信息的逻辑，例如通过I2C或SPI协议从STM32微控制器读取数据
    // 由于这是一个示例，我们暂时返回一个默认的FrigeratorHistoryInfo对象
    return MessageReceverManager::GetInstance().GetFrigeratorHistoryInfo();
}