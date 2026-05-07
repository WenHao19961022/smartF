#include <thread>
#include <chrono>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include "../include/message_recever_manager.h"

// POSIX串口头文件（Jetson/Linux平台）
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

MessageReceverManager& MessageReceverManager::GetInstance() {
    static MessageReceverManager instance;
    return instance;
}

MessageReceverManager::MessageReceverManager() {
    ReceverInit();
}

MessageReceverManager::~MessageReceverManager() {
    if (m_serialFd >= 0) {
        close(m_serialFd);
        m_serialFd = -1;
    }
}

bool MessageReceverManager::InitSerial() {
    m_serialFd = open(m_serialPort.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (m_serialFd < 0) {
        std::cerr << "[STM32] Failed to open serial port: " << m_serialPort
                  << " (" << strerror(errno) << ")" << std::endl;
        return false;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(m_serialFd, &tty) != 0) {
        std::cerr << "[STM32] tcgetattr failed: " << strerror(errno) << std::endl;
        close(m_serialFd);
        m_serialFd = -1;
        return false;
    }

    // 设置波特率
    speed_t baud;
    switch (m_baudrate) {
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

    if (tcsetattr(m_serialFd, TCSANOW, &tty) != 0) {
        std::cerr << "[STM32] tcsetattr failed: " << strerror(errno) << std::endl;
        close(m_serialFd);
        m_serialFd = -1;
        return false;
    }

    tcflush(m_serialFd, TCIFLUSH);

    std::cout << "[STM32] Serial port opened: " << m_serialPort
              << " @ " << m_baudrate << " baud" << std::endl;
    return true;
}

void MessageReceverManager::ReceverInit() {
    m_initStatus.store(INITI_UNFINISHED);

    m_dataMutex.lock();
    m_historyInfo = {};
    m_dataMutex.unlock();

    // 初始化串口
    m_useSimulation = false;
    if (!InitSerial()) {
        std::cerr << "[STM32] Serial init failed, switching to simulation mode" << std::endl;
        m_useSimulation = true;
    }

    ReceverReady();
}

void MessageReceverManager::MainLoop() {
    while (true) {
        FrigeratorInfoWithTimestamp latestInfo = GetLatestFrigeratorInfo();
        UpdateFrigeratorHistoryInfo(latestInfo);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

FrigeratorHistoryInfo MessageReceverManager::GetFrigeratorHistoryInfo() {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_historyInfo;
}

bool MessageReceverManager::ReadFromSerial(std::vector<uint8_t>& buffer) {
    if (m_serialFd < 0) return false;

    uint8_t tempBuf[256];
    ssize_t bytesRead = read(m_serialFd, tempBuf, sizeof(tempBuf));

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
        if (data[i] != FRAME_HEAD) {
            i++;
            continue;
        }

        // 检查是否有足够的数据
        if (i + FRAME_MIN_LEN > data.size()) {
            break;
        }

        uint8_t len = data[i + 1];

        // 验证帧尾
        if (data[i + 2 + len + 1] != FRAME_TAIL) {
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
                ? DoorStatus::DOOR_OPEN
                : DoorStatus::DOOR_CLOSED;

            result.timestamp = static_cast<uint32_t>(std::time(nullptr));
            return true;
        }

        i += 2 + len + 2;  // 跳过完整帧
    }

    return false;
}

void MessageReceverManager::GenerateSimulatedData(FrigeratorInfoWithTimestamp& info) {
    m_simCounter++;

    info.timestamp = static_cast<uint32_t>(std::time(nullptr));

    // 模拟冰箱温度在3-7°C之间波动
    info.info.temperature = static_cast<uint16_t>(40 + (rand() % 30));  // 4.0-6.9°C (×10)

    // 模拟湿度在60-90%之间波动
    info.info.humidity = static_cast<uint16_t>(700 + (rand() % 200));   // 70.0-89.9% (×10)

    // 模拟重量
    info.info.weight = static_cast<uint16_t>(2000 + (rand() % 500));

    // 模拟门状态：每50次循环切换一次
    info.info.doorStatus = ((m_simCounter / 50) % 2 == 1)
        ? DoorStatus::DOOR_OPEN
        : DoorStatus::DOOR_CLOSED;
}

FrigeratorInfoWithTimestamp MessageReceverManager::GetLatestFrigeratorInfo() {
    FrigeratorInfoWithTimestamp latestInfo = {};

    if (m_useSimulation) {
        // 模拟模式
        GenerateSimulatedData(latestInfo);
    } else {
        // 串口模式：从STM32读取数据
        std::vector<uint8_t> serialBuf;
        if (ReadFromSerial(serialBuf)) {
            if (!ParseSerialData(serialBuf, latestInfo)) {
                // 解析失败，返回空数据
                latestInfo.timestamp = static_cast<uint32_t>(std::time(nullptr));
                latestInfo.info = {};
            }
        } else {
            // 读取失败，返回空数据
            latestInfo.timestamp = static_cast<uint32_t>(std::time(nullptr));
            latestInfo.info = {};
        }
    }

    return latestInfo;
}

void MessageReceverManager::UpdateFrigeratorHistoryInfo(FrigeratorInfoWithTimestamp& newInfo) {
    std::lock_guard<std::mutex> lock(m_dataMutex);

    // 门状态变化时记录
    if (m_historyInfo.doorStatus[FRIGERATOR_HISTORY_INFO_SIZE - 1] != newInfo.info.doorStatus) {
        for (size_t i = 0; i < FRIGERATOR_HISTORY_INFO_SIZE - 1; i++) {
            m_historyInfo.doorStatusTimestamp[i] = m_historyInfo.doorStatusTimestamp[i + 1];
            m_historyInfo.doorStatus[i] = m_historyInfo.doorStatus[i + 1];
        }
        m_historyInfo.doorStatusTimestamp[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.timestamp;
        m_historyInfo.doorStatus[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.info.doorStatus;
    }

    // 湿度变化超过阈值时记录
    if (std::abs(static_cast<int>(m_historyInfo.humidity[FRIGERATOR_HISTORY_INFO_SIZE - 1])
                 - static_cast<int>(newInfo.info.humidity)) > HUMIDITY_CHANGE_THRESHOLD) {
        for (size_t i = 0; i < FRIGERATOR_HISTORY_INFO_SIZE - 1; i++) {
            m_historyInfo.humidityTimestamp[i] = m_historyInfo.humidityTimestamp[i + 1];
            m_historyInfo.humidity[i] = m_historyInfo.humidity[i + 1];
        }
        m_historyInfo.humidityTimestamp[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.timestamp;
        m_historyInfo.humidity[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.info.humidity;
    }

    // 温度变化超过阈值时记录
    if (std::abs(static_cast<int>(m_historyInfo.temperature[FRIGERATOR_HISTORY_INFO_SIZE - 1])
                 - static_cast<int>(newInfo.info.temperature)) > TEMPERATURE_CHANGE_THRESHOLD) {
        for (size_t i = 0; i < FRIGERATOR_HISTORY_INFO_SIZE - 1; i++) {
            m_historyInfo.temperatureTimestamp[i] = m_historyInfo.temperatureTimestamp[i + 1];
            m_historyInfo.temperature[i] = m_historyInfo.temperature[i + 1];
        }
        m_historyInfo.temperatureTimestamp[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.timestamp;
        m_historyInfo.temperature[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.info.temperature;
    }

    // 重量变化超过阈值时记录
    if (std::abs(static_cast<int>(m_historyInfo.weight[FRIGERATOR_HISTORY_INFO_SIZE - 1])
                 - static_cast<int>(newInfo.info.weight)) > WEIGHT_CHANGE_THRESHOLD) {
        for (size_t i = 0; i < FRIGERATOR_HISTORY_INFO_SIZE - 1; i++) {
            m_historyInfo.weightTimestamp[i] = m_historyInfo.weightTimestamp[i + 1];
            m_historyInfo.weight[i] = m_historyInfo.weight[i + 1];
        }
        m_historyInfo.weightTimestamp[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.timestamp;
        m_historyInfo.weight[FRIGERATOR_HISTORY_INFO_SIZE - 1] = newInfo.info.weight;
    }
}