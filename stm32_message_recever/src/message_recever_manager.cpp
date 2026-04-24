#include <thread>
#include "../include/message_recever_manager.h"
#include <iterator>

MessageReceverManager& MessageReceverManager::GetInstance()
{
    static MessageReceverManager instance;
    return instance;
}

MessageReceverManager::MessageReceverManager()
{
    ReceverInit();
}

MessageReceverManager::~MessageReceverManager()
{
    ReceverInit();
}

void MessageReceverManager::ReceverInit()
{
    m_initFinished.store(INITI_UNFINISHED);

    m_dataMutex.lock();
    m_historyInfo = {};
    m_dataMutex.unlock();

    ReceverReady();
    return;
}

void MessageReceverManager::MainLoop()
{
    while (true)
    {
        FrigeratorInfoWithTimestamp latestInfo = GetLatestFrigeratorInfo();
        if (IsFrigeratorInfoChanged(latestInfo)) {
            UpdateFrigeratorHistoryInfo(latestInfo);
        }
        // 可以添加适当的睡眠时间，避免CPU占用过高
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

FrigeratorHistoryInfo MessageReceverManager::GetFrigeratorHistoryInfo()
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_historyInfo; // 返回当前的冰箱历史信息
}

FrigeratorInfoWithTimestamp MessageReceverManager::GetLatestFrigeratorInfo()
{
    // 这里应该包含实际获取冰箱状态信息的逻辑，例如通过I2C或SPI协议从STM32微控制器读取数据
    // 由于这是一个示例，我们暂时返回一个默认的FrigeratorInfoWithTimestamp对象
    FrigeratorInfoWithTimestamp latestInfo = {};
    latestInfo.timestamp = static_cast<uint32_t>(std::time(nullptr)); // 使用当前时间作为时间戳
    latestInfo.info = {}; // 这里应该包含实际获取的冰箱状态信息
    return latestInfo;
}

bool MessageReceverManager::IsFrigeratorInfoChanged(FrigeratorInfoWithTimestamp& newInfo)
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    // 门的开关状态发生变化，或者水果重量变化超过100g，或者温度变化超过1度，或者湿度变化超过5%，都认为冰箱状态发生了变化
    const FrigeratorInfoWithTimestamp& lastInfo = m_historyInfo.frigeratorInfoWithTimestamp[FRIGERATOR_HISTORY_INFO_SIZE - 1]; // 获取最新的状态信息
    if (lastInfo.info.doorStatus != newInfo.info.doorStatus ||
        std::abs(static_cast<int>(lastInfo.info.weight) - static_cast<int>(newInfo.info.weight)) > WEIGHT_CHANGE_THRESHOLD ||
        std::abs(static_cast<int>(lastInfo.info.temperature) - static_cast<int>(newInfo.info.temperature)) > TEMPERATURE_CHANGE_THRESHOLD ||
        std::abs(static_cast<int>(lastInfo.info.humidity) - static_cast<int>(newInfo.info.humidity)) > HUMIDITY_CHANGE_THRESHOLD) {
        return true; // 冰箱状态发生了变化
    }
    return false; // 冰箱状态没有发生变化
}

void MessageReceverManager::UpdateFrigeratorHistoryInfo(FrigeratorInfoWithTimestamp& newInfo)
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    // 将新的冰箱状态信息插入到历史信息数组的末尾，其他信息依次向前移动
    for (size_t i = 0; i < FRIGERATOR_HISTORY_INFO_SIZE - 1; ++i) {
        m_historyInfo.frigeratorInfoWithTimestamp[i] = m_historyInfo.frigeratorInfoWithTimestamp[i + 1];
    }
    m_historyInfo.frigeratorInfoWithTimestamp[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo; // 将新的冰箱状态信息放在数组的末尾
}

