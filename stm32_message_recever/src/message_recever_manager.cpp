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
    mSerialFd = open(mSerialPort.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);

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
    tty.c_cc[VMIN] = 0;
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

bool MessageReceverManager::ReadFromSerial(std::vector<uint8_t>& buffer) {
    if (mSerialFd < 0) return false;

    uint8_t tempBuf[256];
    ssize_t bytesRead = read(mSerialFd, tempBuf, sizeof(tempBuf));

    if (bytesRead > 0) {
        buffer.assign(tempBuf, tempBuf + bytesRead);
        return true;
    }

    return false;
}

bool MessageReceverManager::ParseSerialData(
    const std::vector<uint8_t>& data,
    FrigeratorInfoWithTimestamp& result)
{
    /*
     * STM32通信协议帧格式：
     * [0]     HEAD      = 0xAA
     * [1]     LEN       = 数据长度
     * [2]     CMD       = 命令字
     * [3..N-2] DATA     = 数据域
     * [N-1]   CHECKSUM  = 校验和（XOR）
     * [N]     TAIL      = 0x55
     *
     * 数据域（LEN=6时）：
     * [0-1]   温度 (uint16, ×10, 如 49 = 4.9°C)
     * [2-3]   湿度 (uint16, ×10, 如 805 = 80.5%)
     * [4-5]   重量 (uint16, 单位g)
     * [6]     门状态 (0=关, 1=开)
     */

    size_t i = 0;
    while (i < data.size()) {
        // 查找帧头
        if (data[i] != kFrameHead) {
            i++;
            continue;
        }

        // 检查是否有足够的数据
        if (i + kFrameMinLen > data.size()) {
            break;
        }

        uint8_t len = data[i + 1];

        // 验证帧尾
        if (data[i + 2 + len + 1] != kFrameTail) {
            i++;
            continue;
        }

        // 验证校验和
        uint8_t checksum = 0;
        for (size_t j = i + 2; j < i + 2 + len; j++) {
            checksum ^= data[j];
        }
        if (checksum != data[i + 2 + len]) {
            i++;
            continue;
        }

        // 解析数据域
        if (len >= 7) {
            size_t base = i + 3;

            result.info.temperature = static_cast<uint16_t>(
                data[base] | (data[base + 1] << 8));
            result.info.humidity = static_cast<uint16_t>(
                data[base + 2] | (data[base + 3] << 8));
            result.info.weight = static_cast<uint16_t>(
                data[base + 4] | (data[base + 5] << 8));
            result.info.doorStatus = (data[base + 6] == 1)
                ? DoorStatus::DoorOpen
                : DoorStatus::DoorClosed;

            result.timestamp = static_cast<uint32_t>(std::time(nullptr));
            return true;
        }

        i += 2 + len + 2;  // 跳过完整帧
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
            LOG_PRINT("[Stm32]", "Parsed successfully: temp=" << (latestInfo.info.temperature / 10.0)
                      << "C | humidity=" << (latestInfo.info.humidity / 10.0)
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
