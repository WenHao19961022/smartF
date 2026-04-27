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
    m_initStatus.store(INITI_UNFINISHED);

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
        UpdateFrigeratorHistoryInfo(latestInfo);
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

void MessageReceverManager::UpdateFrigeratorHistoryInfo(FrigeratorInfoWithTimestamp& newInfo)
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    // 将新的冰箱状态信息添加到历史信息中，保持历史信息的大小
    if(m_historyInfo.doorStatus[FRIGERATOR_HISTORY_INFO_SIZE - 1] != newInfo.info.doorStatus)
    {
        for (size_t i = 0; i < FRIGERATOR_HISTORY_INFO_SIZE - 1; i++)
        {
            m_historyInfo.doorStatusTimestamp[i] = m_historyInfo.doorStatusTimestamp[i + 1];
            m_historyInfo.doorStatus[i] = m_historyInfo.doorStatus[i + 1];
        }
        m_historyInfo.doorStatusTimestamp[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.timestamp;
        m_historyInfo.doorStatus[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.info.doorStatus;
    }
    // 湿度变化超过阈值HUMIDITY_CHANGE_THRESHOLD时记录新的状态信息
    if(std::abs(m_historyInfo.humidity[FRIGERATOR_HISTORY_INFO_SIZE - 1] - newInfo.info.humidity) > HUMIDITY_CHANGE_THRESHOLD)
    {
        for (size_t i = 0; i < FRIGERATOR_HISTORY_INFO_SIZE - 1; i++)
        {
            m_historyInfo.humidityTimestamp[i] = m_historyInfo.humidityTimestamp[i + 1];
            m_historyInfo.humidity[i] = m_historyInfo.humidity[i + 1];
        }
        m_historyInfo.humidityTimestamp[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.timestamp;
        m_historyInfo.humidity[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.info.humidity;
    }
    // 温度变化超过阈值TEMPERATURE_CHANGE_THRESHOLD时记录新的状态信息
    if(std::abs(m_historyInfo.temperature[FRIGERATOR_HISTORY_INFO_SIZE - 1] - newInfo.info.temperature) > TEMPERATURE_CHANGE_THRESHOLD)
    {
        for (size_t i = 0; i < FRIGERATOR_HISTORY_INFO_SIZE - 1; i++)
        {
            m_historyInfo.temperatureTimestamp[i] = m_historyInfo.temperatureTimestamp[i + 1];
            m_historyInfo.temperature[i] = m_historyInfo.temperature[i + 1];
        }
        m_historyInfo.temperatureTimestamp[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.timestamp;
        m_historyInfo.temperature[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.info.temperature;
    }
    // 重量变化超过阈值WEIGHT_CHANGE_THRESHOLD时记录新的状态信息
    if(std::abs(m_historyInfo.weight[FRIGERATOR_HISTORY_INFO_SIZE - 1] - newInfo.info.weight) > WEIGHT_CHANGE_THRESHOLD)
    {
        for (size_t i = 0; i < FRIGERATOR_HISTORY_INFO_SIZE - 1; i++)
        {
            m_historyInfo.weightTimestamp[i] = m_historyInfo.weightTimestamp[i + 1];
            m_historyInfo.weight[i] = m_historyInfo.weight[i + 1];
        }
        m_historyInfo.weightTimestamp[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.timestamp;
        m_historyInfo.weight[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.info.weight;
    }
    return;
}

