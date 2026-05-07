#ifndef MESSAGE_RECEVER_MANAGER_H
#define MESSAGE_RECEVER_MANAGER_H

#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include "../api/stm32_message_recever_api.h"

// 串口配置常量（可在配置文件中覆盖）
const static bool INITI_FINISHED = true;
const static bool INITI_UNFINISHED = false;
const static uint16_t WEIGHT_CHANGE_THRESHOLD = 100;
const static uint16_t TEMPERATURE_CHANGE_THRESHOLD = 1;
const static uint16_t HUMIDITY_CHANGE_THRESHOLD = 5;

// 默认串口配置
const static std::string DEFAULT_SERIAL_PORT = "/dev/ttyUSB0";
const static int DEFAULT_BAUDRATE = 115200;
const static int SERIAL_TIMEOUT_MS = 1000;

// STM32通信协议配置
const static uint8_t FRAME_HEAD = 0xAA;
const static uint8_t FRAME_TAIL = 0x55;
const static uint8_t FRAME_MIN_LEN = 10;

// 冰箱数据类型
enum class FridgeDataType : uint8_t {
    TEMPERATURE = 0x01,
    HUMIDITY = 0x02,
    WEIGHT = 0x03,
    DOOR_STATUS = 0x04
};

class MessageReceverManager
{
public:
    static MessageReceverManager& GetInstance();

    void ReceverInit();
    bool IsReceverReady() const { return m_initStatus.load(); }
    void MainLoop();
    FrigeratorHistoryInfo GetFrigeratorHistoryInfo();

    // 串口配置接口
    void SetSerialPort(const std::string& port) { m_serialPort = port; }
    void SetBaudrate(int baudrate) { m_baudrate = baudrate; }

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

    void ReceverReady() { m_initStatus.store(INITI_FINISHED); }

    std::atomic<bool> m_initStatus{INITI_UNFINISHED};
    std::mutex m_dataMutex;
    FrigeratorHistoryInfo m_historyInfo;

    // 串口相关
    std::string m_serialPort = DEFAULT_SERIAL_PORT;
    int m_baudrate = DEFAULT_BAUDRATE;
    int m_serialFd = -1;  // 串口文件描述符

    // 模拟数据计数器（用于测试）
    uint32_t m_simCounter = 0;
    bool m_useSimulation = false;  // 串口打开失败时自动切换到模拟模式
};

#endif // MESSAGE_RECEVER_MANAGER_H