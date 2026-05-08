#include <iostream>
#include <thread>
#include "../include/external_apis.h"
#include "../include/core_manager.h"
#include <config_manager.h>
#include "../include/core_log.h"
#include <functional>

// ==================== Mock STM32 Mode ====================
// 用于联调测试，替代真实的STM32串口通信
// 模拟门开关信号：默认开门5秒，关门10秒，循环往复
void MockStm32Thread() {
    LOG_START(std::string("MockStm32 running in thread ") + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
    StartMockStm32Mode(5, 10);
}

// 模块函数声明
void LaunchMqttMessageSender() {
    LOG_START(std::string("MqttMessageSender running in thread ") + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
    MqttMessageSenderMainLoop();
}

void LaunchStm32MessageReceiver() {
    LOG_START(std::string("Stm32MessageReceiver running in thread ") + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
    Stm32MessageReceverMainLoop();
}

void LaunchCvModel() {
    LOG_START(std::string("CvModel running in thread ") + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));

    if (!CvModelInit()) {
        LOG_ERR("CvModel initialization failed. Exiting CvModel thread.");
        return;
    }

    CvModelMainLoop();
}

void CoreThread() {
    LOG_START(std::string("Core running in thread ") + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
    CoreManager manager;
    manager.Init();
    manager.Run();
}

// 联调测试开关：设为 true 启用 Mock STM32，设为 false 使用真实串口
#ifdef USE_MOCK_STM32
bool gUseMockStm32 = true;
#else
bool gUseMockStm32 = false;
#endif

int main() {
    ConfigManager::GetInstance().LoadConfig();

    // 创建4个线程，每个线程运行一个模块
    std::thread t1(LaunchMqttMessageSender);

    if (gUseMockStm32) {
        std::thread t2(MockStm32Thread);  // 使用 Mock STM32
        std::thread t3(LaunchCvModel);
        std::thread t4(CoreThread);

        t1.join();
        t2.join();
        t3.join();
        t4.join();
    } else {
        std::thread t2(LaunchStm32MessageReceiver);  // 使用真实串口
        std::thread t3(LaunchCvModel);
        std::thread t4(CoreThread);

        t1.join();
        t2.join();
        t3.join();
        t4.join();
    }

    LOG_OK("All modules finished.");
    return 0;
}