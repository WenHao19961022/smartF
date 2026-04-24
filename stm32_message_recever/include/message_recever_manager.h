#ifndef MESSAGE_RECEVER_MANAGER_H
#define MESSAGE_RECEVER_MANAGER_H

#include <mutex>
#include <atomic>
#include "../api/stm32_message_recever_api.h" // 包含stm32_message_recever_api.h头文件以使用其中定义的结构体和函数声明

const static bool INITI_FINISHED = true; // 定义一个常量，表示初始化是否完成
const static bool INITI_UNFINISHED = false; // 定义一个常量，表示初始化是否未完成
const uint16_t WEIGHT_CHANGE_THRESHOLD = 100; // 定义一个常量，表示记录冰箱状态信息的重量变化阈值
const uint16_t TEMPERATURE_CHANGE_THRESHOLD = 1; // 定义一个常量，表示记录冰箱状态信息的温度变化阈值
const uint16_t HUMIDITY_CHANGE_THRESHOLD = 5; // 定义一个常量，表示记录冰箱状态信息的湿度变化阈值

class MessageReceverManager
{
public:
    static MessageReceverManager& GetInstance(); // 获取MessageReceverManager实例的静态方法
    void ReceverInit(); // 初始化方法，建议在程序启动时显式调用一次
    bool IsReceverReady() const { return m_initFinished.load(); } // 检查接收器是否准备就绪，供与core通讯
    void MainLoop(); // 接收功能主循环
    FrigeratorHistoryInfo GetFrigeratorHistoryInfo(); // 获取冰箱历史信息的方法，供与core通讯

private:
    MessageReceverManager(); // 私有构造函数，防止外部实例化
    ~MessageReceverManager(); // 析构函数
    MessageReceverManager(const MessageReceverManager&) = delete; // 禁止复制构造函数，确保单例模式
    MessageReceverManager& operator=(const MessageReceverManager&) = delete; // 禁止赋值运算符，确保单例模式

    void ReceverReady() { m_initFinished.store(INITI_FINISHED); } // 设置接收器准备就绪的标志位

    FrigeratorInfoWithTimestamp GetLatestFrigeratorInfo(); // 获取最新的冰箱状态信息的方法
    bool IsFrigeratorInfoChanged(FrigeratorInfoWithTimestamp& newInfo); // 判断冰箱状态信息是否发生变化的方法
    void UpdateFrigeratorHistoryInfo(FrigeratorInfoWithTimestamp& newInfo); // 更新冰箱历史信息的方法

    // 数据
    std::atomic<bool> m_initFinished{INITI_UNFINISHED};
    std::mutex m_dataMutex;
    FrigeratorHistoryInfo m_historyInfo; // 冰箱历史信息,最后一个信息是最新的状态信息
};

#endif // MESSAGE_RECEVER_MANAGER_H