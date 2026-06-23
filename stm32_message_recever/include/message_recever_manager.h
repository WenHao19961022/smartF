#ifndef MESSAGE_RECEVER_MANAGER_H
#define MESSAGE_RECEVER_MANAGER_H

#include <mutex>
#include <atomic>
#include <string>
#include "thread"
#include <vector>
#include "../api/stm32_message_recever_api.h"

// ==================== 常量 ====================
const bool kInitFinished = true;
const bool kInitUnfinished = false;

const uint8_t kFrameHead = 0xAA;
const uint8_t kFrameTail = 0x55;
const uint8_t kFrameMinLen = 10;

// ==================== 消息接收管理类 ====================
class MessageReceverManager {
public:
    static MessageReceverManager& GetInstance();

    void Init();
    bool IsReady() const { return mInitStatus.load(); }
    void MainLoop();
    FrigeratorHistoryInfo GetFrigeratorHistoryInfo();

    // Mock mode: 模拟STM32信号（用于联调测试，无真实硬件时启用）
    void StartMockMode(int doorOpenDurationSec = 5, int doorClosedDurationSec = 10);
    void StopMockMode();

private:
    MessageReceverManager();
    ~MessageReceverManager();

    MessageReceverManager(const MessageReceverManager&) = delete;
    MessageReceverManager& operator=(const MessageReceverManager&) = delete;

    bool InitSerial();
    bool ReadFromSerial(std::vector<uint8_t>& buffer);
    bool ParseSerialData(const std::vector<uint8_t>& data, FrigeratorInfoWithTimestamp& result);
    FrigeratorInfoWithTimestamp GetLatestFrigeratorInfo();
    void UpdateFrigeratorHistoryInfo(FrigeratorInfoWithTimestamp& newInfo);
    void SetReady() { mInitStatus.store(kInitFinished); }

    void MockThreadFunc(int doorOpenDurationSec, int doorClosedDurationSec);

    std::atomic<bool> mInitStatus{kInitUnfinished};
    std::mutex mDataMutex;
    FrigeratorHistoryInfo mHistoryInfo;

    std::string mSerialPort = "/dev/ttyTHS0";
    int mBaudrate = 115200;
    int mSerialFd = -1;

    // 变化阈值（从配置管理器读取）
    uint16_t mWeightChangeThreshold = 3;
    uint16_t mTemperatureChangeThreshold = 1;
    uint16_t mHumidityChangeThreshold = 5;

    // Mock mode
    std::atomic<bool> mMockRunning{false};
    std::thread mMockThread;
};

#endif // MESSAGE_RECEVER_MANAGER_H
