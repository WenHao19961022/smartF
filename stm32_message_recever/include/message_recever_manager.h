#ifndef MESSAGE_RECEVER_MANAGER_H
#define MESSAGE_RECEVER_MANAGER_H

#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include "../api/stm32_message_recever_api.h"

// ==================== 常量 ====================
const bool kInitFinished = true;
const bool kInitUnfinished = false;
const uint16_t kWeightChangeThreshold = 100;
const uint16_t kTemperatureChangeThreshold = 1;
const uint16_t kHumidityChangeThreshold = 5;

const std::string kDefaultSerialPort = "/dev/ttyUSB0";
const int kDefaultBaudrate = 115200;
const int kSerialTimeoutMs = 1000;

const uint8_t kFrameHead = 0xAA;
const uint8_t kFrameTail = 0x55;
const uint8_t kFrameMinLen = 10;

// ==================== 枚举 ====================
enum class FridgeDataType : uint8_t {
    Temperature = 0x01,
    Humidity    = 0x02,
    Weight      = 0x03,
    DoorStatus  = 0x04
};

// ==================== 消息接收管理类 ====================
class MessageReceverManager {
public:
    static MessageReceverManager& GetInstance();

    void Init();
    bool IsReady() const { return mInitStatus.load(); }
    void MainLoop();
    FrigeratorHistoryInfo GetFrigeratorHistoryInfo();

    void SetSerialPort(const std::string& port) { mSerialPort = port; }
    void SetBaudrate(int baudrate) { mBaudrate = baudrate; }

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
    void GenerateSimulatedData(FrigeratorInfoWithTimestamp& info);
    void SetReady() { mInitStatus.store(kInitFinished); }

    std::atomic<bool> mInitStatus{kInitUnfinished};
    std::mutex mDataMutex;
    FrigeratorHistoryInfo mHistoryInfo;

    std::string mSerialPort = kDefaultSerialPort;
    int mBaudrate = kDefaultBaudrate;
    int mSerialFd = -1;

    uint32_t mSimCounter = 0;
    bool mUseSimulation = false;
};

#endif // MESSAGE_RECEVER_MANAGER_H
