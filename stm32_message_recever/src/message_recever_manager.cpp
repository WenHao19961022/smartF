#include <thread>
#include <chrono>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include "../include/message_recever_manager.h"
#include "../../common/include/logger.h"
#include "../../common/include/config_manager.h"

// POSIX串口头文件（Jetson/Linux平台）
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <iomanip>

MessageReceverManager& MessageReceverManager::GetInstance() {
    static MessageReceverManager instance;
    return instance;
}

MessageReceverManager::MessageReceverManager() {
    Init();
}

MessageReceverManager::~MessageReceverManager() {
    if (mSerialFd >= 0) {
        close(mSerialFd);
        mSerialFd = -1;
    }
}

bool MessageReceverManager::InitSerial() {
    mSerialFd = open(mSerialPort.c_str(), O_RDWR | O_NOCTTY);

    if (mSerialFd < 0) {
        LOG_PRINT("[Stm32]", "Failed to open serial port: " << mSerialPort << " (" << strerror(errno) << ")");
        return false;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(mSerialFd, &tty) != 0) {
        LOG_PRINT("[Stm32]", "tcgetattr failed: " << strerror(errno));
        close(mSerialFd);
        mSerialFd = -1;
        return false;
    }

    // 设置波特率
    speed_t baud;
    switch (mBaudrate) {
        case 9600:   baud = B9600;   break;
        case 19200:  baud = B19200;  break;
        case 38400:  baud = B38400;  break;
        case 57600:  baud = B57600;  break;
        case 115200: baud = B115200; break;
        default:     baud = B115200; break;
    }
    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);

    // 8N1配置
    tty.c_cflag &= ~PARENB;         // 无校验
    tty.c_cflag &= ~CSTOPB;         // 1停止位
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;             // 8数据位
    tty.c_cflag &= ~CRTSCTS;        // 无硬件流控
    tty.c_cflag |= CREAD | CLOCAL;  // 启用接收，忽略控制线

    // 原始输入模式
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    // 原始输出模式
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;

    // 超时设置
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;  // 0.1秒超时

    if (tcsetattr(mSerialFd, TCSANOW, &tty) != 0) {
        LOG_PRINT("[Stm32]", "tcsetattr failed: " << strerror(errno));
        close(mSerialFd);
        mSerialFd = -1;
        return false;
    }

    tcflush(mSerialFd, TCIFLUSH);

    LOG_PRINT("[Stm32]", "Serial port opened: " << mSerialPort << " @ " << mBaudrate << " baud");
    return true;
}

void MessageReceverManager::Init() {
    LOG_PRINT("[Stm32]", "=== MessageReceverManager Init START ===");
    mInitStatus.store(kInitUnfinished);

    mDataMutex.lock();
    mHistoryInfo = {};
    mDataMutex.unlock();

    // 从配置管理器读取串口参数和变化阈值
    mSerialPort = ConfigManager::GetInstance().GetString("serial.port", "/dev/ttyTHS0");
    mBaudrate = ConfigManager::GetInstance().GetInt("serial.baudrate", 115200);
    mWeightChangeThreshold = static_cast<uint16_t>(
        ConfigManager::GetInstance().GetInt("serial.weight_threshold", 100));
    mTemperatureChangeThreshold = static_cast<uint16_t>(
        ConfigManager::GetInstance().GetInt("serial.temperature_threshold", 1));
    mHumidityChangeThreshold = static_cast<uint16_t>(
        ConfigManager::GetInstance().GetInt("serial.humidity_threshold", 5));

    LOG_PRINT("[Stm32]", "Config loaded:");
    LOG_PRINT("[Stm32]", "  - serial_port: " << mSerialPort);
    LOG_PRINT("[Stm32]", "  - baudrate: " << mBaudrate);
    LOG_PRINT("[Stm32]", "  - weight_threshold: " << mWeightChangeThreshold << "g");
    LOG_PRINT("[Stm32]", "  - temperature_threshold: " << mTemperatureChangeThreshold << " (raw units)");
    LOG_PRINT("[Stm32]", "  - humidity_threshold: " << mHumidityChangeThreshold << " (raw units)");

    // 初始化串口
    LOG_PRINT("[Stm32]", "Attempting to open serial port...");
    if (!InitSerial()) {
        LOG_PRINT("[Stm32]", "Serial init failed, module will still be marked as ready");
        SetReady(); // 仍然标记为就绪（表示模块已初始化，只是串口不可用）
        LOG_PRINT("[Stm32]", "=== MessageReceverManager Init END (serial unavailable) ===");
        return;
    }

    SetReady();
    LOG_PRINT("[Stm32]", "=== MessageReceverManager Init END (serial available) ===");
}

void MessageReceverManager::MainLoop() {
    LOG_PRINT("[Stm32]", "=== MessageReceverManager MainLoop START ===");
    int loopCount = 0;
    while (true) {
        FrigeratorInfoWithTimestamp latestInfo = GetLatestFrigeratorInfo();
        UpdateFrigeratorHistoryInfo(latestInfo);

        loopCount++;
        if (loopCount % 100 == 0) {
            LOG_PRINT("[Stm32]", "MainLoop heartbeat #" << loopCount
                      << " | latest: temp=" << (latestInfo.info.temperature / 10.0) << "C"
                      << " humidity=" << (latestInfo.info.humidity / 10.0) << "%"
                      << " weight=" << latestInfo.info.weight << "g"
                      << " door=" << (int)latestInfo.info.doorStatus);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    LOG_PRINT("[Stm32]", "=== MessageReceverManager MainLoop END ===");
}

FrigeratorHistoryInfo MessageReceverManager::GetFrigeratorHistoryInfo() {
    std::lock_guard<std::mutex> lock(mDataMutex);
    return mHistoryInfo;
}

bool MessageReceverManager::ReadFromSerial(std::vector<uint8_t>& buffer)
{
    if (mSerialFd < 0)
        return false;

    uint8_t tempBuf[256];

    ssize_t bytesRead = read(mSerialFd, tempBuf, sizeof(tempBuf));

    if (bytesRead > 0)
    {
        buffer.assign(tempBuf, tempBuf + bytesRead);

        std::ostringstream oss;

        oss << "Read " << bytesRead << " bytes: ";

        for (int i = 0; i < bytesRead; i++)
        {
            oss << std::hex
                << std::setw(2)
                << std::setfill('0')
                << static_cast<int>(tempBuf[i])
                << " ";
        }

        LOG_PRINT("[Stm32]", oss.str());

        return true;
    }

    return false;
}

bool MessageReceverManager::ParseSerialData(
    const std::vector<uint8_t>& data,
    FrigeratorInfoWithTimestamp& result)
{
    /*
     * STM32通信协议精准映射（基于真实抓包数据）：
     * [0]    HEAD      = 0xAA
     * [1]    LEN       = 0x08 (参与校验和计算的字节数，即 CMD + DATA 的总长度)
     * [2]    CMD       = 命令字 (0x01)
     * ---------------- DATA 域 (LEN = 8) ----------------
     * [3]    base[0]   = 温度 (单字节, 0x1D = 29°C)
     * [4]    base[1]   = 占位/未知 (0x00)
     * [5]    base[2]   = 湿度 (单字节, 0x32 = 50%)
     * [6]    base[3]   = 占位/未知 (0x00)
     * [7-8]  base[4-5] = 重量 (uint16_t, 大端序, 当前 0x0001 = 1g)
     * [9]    base[6]   = 门状态 (单字节, 0=关, 1=开)
     * --------------------------------------------------
     * [10]   CHECKSUM  = 校验和（从 CMD[2] 开始，连续异或 LEN[1] 个字节）
     * [11]   TAIL      = 0x55
     */

    size_t i = 0;
    while (i < data.size()) {
        // 1. 寻找帧头
        if (data[i] != kFrameHead) {
            i++;
            continue;
        }

        // 2. 边界检查：确保至少有 HEAD(1) + LEN(1) 两个字节用来读取长度
        if (i + 2 > data.size()) {
            break;
        }

        uint8_t len = data[i + 1]; // len = 8
        size_t frameLen = 2 + len + 2; // HEAD(1) + LEN(1) + DATA_BLOCK(8) + CHECK(1) + TAIL(1) = 12

        // 3. 检查当前缓冲区内整帧数据是否完整
        if (i + frameLen > data.size()) {
            break;
        }

        // 4. 验证帧尾
        if (data[i + frameLen - 1] != kFrameTail) {
            i++;
            continue;
        }

        // 5. 验证校验和（从 CMD 字节开始，向后异或 len 个字节）
        uint8_t checksum = 0;
        size_t checksumStart = i + 2; // CMD 的位置
        for (size_t j = 0; j < len; j++) {
            checksum ^= data[checksumStart + j];
        }
        
        if (checksum != data[i + 2 + len]) {
            // 校验失败，说明不是有效帧
            i++;
            continue;
        }

        // 6. 校验通过，精准提取数据
        size_t base = i + 3; // DATA 域的起始点 (即温度所在位置)

        // 严格按照你的抓包映射赋值
        result.info.temperature = static_cast<float>(data[base]);       // 0x1d -> 29.0C
        result.info.humidity    = static_cast<float>(data[base + 2]);   // 0x32 -> 50.0%
        
        // 重量占用两个字节，采用大端序拼装：(data[base+4] << 8) | data[base+5]
        result.info.weight      = static_cast<uint16_t>((data[base + 4] << 8) | data[base + 5]); // 0x0001 -> 1g
        
        // 门状态在 base + 6
        result.info.doorStatus  = (data[base + 6] == 1) 
            ? DoorStatus::DoorOpen 
            : DoorStatus::DoorClosed;

        // 更新时间戳
        result.timestamp = static_cast<uint32_t>(std::time(nullptr));
        return true;
    }

    return false;
}

FrigeratorInfoWithTimestamp MessageReceverManager::GetLatestFrigeratorInfo() {
    FrigeratorInfoWithTimestamp latestInfo = {};

    std::vector<uint8_t> serialBuf;
    if (ReadFromSerial(serialBuf)) {
        std::ostringstream hexDump;
        hexDump << "Read " << serialBuf.size() << " bytes: ";
        for (uint8_t byte : serialBuf) {
            hexDump << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
        }
        LOG_PRINT("[Stm32]", hexDump.str());
        if (ParseSerialData(serialBuf, latestInfo)) {
            LOG_PRINT("[Stm32]", "Parsed successfully: temp=" << (latestInfo.info.temperature)
                      << "C | humidity=" << (latestInfo.info.humidity)
                      << "% | weight=" << latestInfo.info.weight
                      << "g | door=" << (int)latestInfo.info.doorStatus
                      << " | ts=" << latestInfo.timestamp);
        } else {
            latestInfo.timestamp = static_cast<uint32_t>(std::time(nullptr));
            latestInfo.info = {};
            LOG_PRINT("[Stm32]", "Failed to parse serial data (read " << serialBuf.size() << " bytes)");
        }
    } else {
        latestInfo.timestamp = static_cast<uint32_t>(std::time(nullptr));
        LOG_PRINT("[Stm32]", "No serial data available");
        latestInfo.info = {};
    }

    return latestInfo;
}

void MessageReceverManager::UpdateFrigeratorHistoryInfo(FrigeratorInfoWithTimestamp& newInfo) {
    std::lock_guard<std::mutex> lock(mDataMutex);

    // 门状态变化时记录
    if (mHistoryInfo.doorStatus[kFridgeHistoryInfoSize - 1] != newInfo.info.doorStatus) {
        LOG_PRINT("[Stm32]", "[HISTORY UPDATE] Door status CHANGED: "
                  << (int)mHistoryInfo.doorStatus[kFridgeHistoryInfoSize - 1] << " -> " << (int)newInfo.info.doorStatus
                  << " | ts=" << newInfo.timestamp);
        for (size_t i = 0; i < kFridgeHistoryInfoSize - 1; i++) {
            mHistoryInfo.doorStatusTimestamp[i] = mHistoryInfo.doorStatusTimestamp[i + 1];
            mHistoryInfo.doorStatus[i] = mHistoryInfo.doorStatus[i + 1];
        }
        mHistoryInfo.doorStatusTimestamp[kFridgeHistoryInfoSize - 1] = newInfo.timestamp;
        mHistoryInfo.doorStatus[kFridgeHistoryInfoSize - 1] = newInfo.info.doorStatus;
    }

    // 湿度变化超过阈值时记录
    int humidityDiff = std::abs(static_cast<int>(mHistoryInfo.humidity[kFridgeHistoryInfoSize - 1])
                 - static_cast<int>(newInfo.info.humidity));
    if (humidityDiff > mHumidityChangeThreshold) {
        LOG_PRINT("[Stm32]", "[HISTORY UPDATE] Humidity CHANGED: "
                  << (mHistoryInfo.humidity[kFridgeHistoryInfoSize - 1] / 10.0) << "% -> "
                  << (newInfo.info.humidity / 10.0) << "%"
                  << " (diff=" << humidityDiff << " > threshold=" << mHumidityChangeThreshold << ")");
        for (size_t i = 0; i < kFridgeHistoryInfoSize - 1; i++) {
            mHistoryInfo.humidityTimestamp[i] = mHistoryInfo.humidityTimestamp[i + 1];
            mHistoryInfo.humidity[i] = mHistoryInfo.humidity[i + 1];
        }
        mHistoryInfo.humidityTimestamp[kFridgeHistoryInfoSize - 1] = newInfo.timestamp;
        mHistoryInfo.humidity[kFridgeHistoryInfoSize - 1] = newInfo.info.humidity;
    }

    // 温度变化超过阈值时记录
    int tempDiff = std::abs(static_cast<int>(mHistoryInfo.temperature[kFridgeHistoryInfoSize - 1])
                 - static_cast<int>(newInfo.info.temperature));
    if (tempDiff > mTemperatureChangeThreshold) {
        LOG_PRINT("[Stm32]", "[HISTORY UPDATE] Temperature CHANGED: "
                  << (mHistoryInfo.temperature[kFridgeHistoryInfoSize - 1] / 10.0) << "C -> "
                  << (newInfo.info.temperature / 10.0) << "C"
                  << " (diff=" << tempDiff << " > threshold=" << mTemperatureChangeThreshold << ")");
        for (size_t i = 0; i < kFridgeHistoryInfoSize - 1; i++) {
            mHistoryInfo.temperatureTimestamp[i] = mHistoryInfo.temperatureTimestamp[i + 1];
            mHistoryInfo.temperature[i] = mHistoryInfo.temperature[i + 1];
        }
        mHistoryInfo.temperatureTimestamp[kFridgeHistoryInfoSize - 1] = newInfo.timestamp;
        mHistoryInfo.temperature[kFridgeHistoryInfoSize - 1] = newInfo.info.temperature;
    }

    // 重量变化超过阈值时记录
    int weightDiff = std::abs(static_cast<int>(mHistoryInfo.weight[kFridgeHistoryInfoSize - 1])
                 - static_cast<int>(newInfo.info.weight));
    if (weightDiff > mWeightChangeThreshold) {
        LOG_PRINT("[Stm32]", "[HISTORY UPDATE] Weight CHANGED: "
                  << mHistoryInfo.weight[kFridgeHistoryInfoSize - 1] << "g -> "
                  << newInfo.info.weight << "g"
                  << " (diff=" << weightDiff << " > threshold=" << mWeightChangeThreshold << "g)");
        for (size_t i = 0; i < kFridgeHistoryInfoSize - 1; i++) {
            mHistoryInfo.weightTimestamp[i] = mHistoryInfo.weightTimestamp[i + 1];
            mHistoryInfo.weight[i] = mHistoryInfo.weight[i + 1];
        }
        mHistoryInfo.weightTimestamp[kFridgeHistoryInfoSize - 1] = newInfo.timestamp;
        mHistoryInfo.weight[kFridgeHistoryInfoSize - 1] = newInfo.info.weight;
    }
}

// ==================== Mock Mode ====================
void MessageReceverManager::StartMockMode(int doorOpenDurationSec, int doorClosedDurationSec) {
    LOG_PRINT("[Stm32-Mock]", "=== Starting Mock Mode ===");
    LOG_PRINT("[Stm32-Mock]", "Door open duration: " << doorOpenDurationSec << "s");
    LOG_PRINT("[Stm32-Mock]", "Door closed duration: " << doorClosedDurationSec << "s");

    // 初始化默认数据
    {
        std::lock_guard<std::mutex> lock(mDataMutex);
        for (int i = 0; i < kFridgeHistoryInfoSize; ++i) {
            mHistoryInfo.temperature[i] = 50;  // 5.0°C
            mHistoryInfo.humidity[i] = 600;    // 60.0%
            mHistoryInfo.weight[i] = 5000;     // 5000g
            mHistoryInfo.doorStatus[i] = DoorStatus::DoorClosed;
            mHistoryInfo.temperatureTimestamp[i] = static_cast<uint32_t>(std::time(nullptr));
            mHistoryInfo.humidityTimestamp[i] = static_cast<uint32_t>(std::time(nullptr));
            mHistoryInfo.weightTimestamp[i] = static_cast<uint32_t>(std::time(nullptr));
            mHistoryInfo.doorStatusTimestamp[i] = static_cast<uint32_t>(std::time(nullptr));
        }
    }

    SetReady();
    mMockRunning.store(true);
    mMockThread = std::thread(&MessageReceverManager::MockThreadFunc, this, doorOpenDurationSec, doorClosedDurationSec);

    LOG_PRINT("[Stm32-Mock]", "Mock mode started successfully");
}

void MessageReceverManager::StopMockMode() {
    LOG_PRINT("[Stm32-Mock]", "=== Stopping Mock Mode ===");
    mMockRunning.store(false);
    if (mMockThread.joinable()) {
        mMockThread.join();
    }
    LOG_PRINT("[Stm32-Mock]", "Mock mode stopped");
}

void MessageReceverManager::MockThreadFunc(int doorOpenDurationSec, int doorClosedDurationSec) {
    LOG_PRINT("[Stm32-Mock]", "Mock thread started");

    // 初始状态：门关闭
    DoorStatus currentDoorStatus = DoorStatus::DoorClosed;
    uint16_t currentWeight = 5000;  // 初始重量 5000g

    int cycleCount = 0;
    while (mMockRunning.load()) {
        cycleCount++;

        // 生成模拟数据
        FrigeratorInfoWithTimestamp mockInfo;
        mockInfo.timestamp = static_cast<uint32_t>(std::time(nullptr));

        // 模拟温度波动（4.5°C ~ 5.5°C）
        mockInfo.info.temperature = 45 + (rand() % 11);

        // 模拟湿度波动（55% ~ 65%）
        mockInfo.info.humidity = 550 + (rand() % 101);

        // 门状态和重量
        mockInfo.info.doorStatus = currentDoorStatus;
        mockInfo.info.weight = currentWeight;

        // 更新历史数据
        UpdateFrigeratorHistoryInfo(mockInfo);

        LOG_PRINT("[Stm32-Mock]", "Cycle #" << cycleCount
                  << " | door=" << (currentDoorStatus == DoorStatus::DoorOpen ? "OPEN" : "CLOSED")
                  << " | weight=" << currentWeight << "g"
                  << " | temp=" << (mockInfo.info.temperature / 10.0) << "C"
                  << " | humidity=" << (mockInfo.info.humidity / 10.0) << "%");

        // 等待当前状态持续时间
        int durationSec = (currentDoorStatus == DoorStatus::DoorOpen)
                          ? doorOpenDurationSec
                          : doorClosedDurationSec;

        for (int sec = 0; sec < durationSec && mMockRunning.load(); ++sec) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // 切换门状态
        if (!mMockRunning.load()) break;

        if (currentDoorStatus == DoorStatus::DoorClosed) {
            // 关门 -> 开门：模拟放入/取出物品，重量变化
            currentDoorStatus = DoorStatus::DoorOpen;
            // 随机增加或减少重量（模拟放入/取出水果）
            int weightChange = (rand() % 2 == 0) ? (rand() % 500 + 100) : -(rand() % 500 + 100);
            currentWeight = static_cast<uint16_t>(std::max(100, static_cast<int>(currentWeight) + weightChange));
            LOG_PRINT("[Stm32-Mock]", "Door OPENING | weight change: +" << weightChange << "g -> " << currentWeight << "g");
        } else {
            // 开门 -> 关门
            currentDoorStatus = DoorStatus::DoorClosed;
            LOG_PRINT("[Stm32-Mock]", "Door CLOSING");
        }
    }

    LOG_PRINT("[Stm32-Mock]", "Mock thread ended");
}
