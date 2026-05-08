#include <iostream>
#include <thread>
#include "../include/external_apis.h"
#include "../include/core_manager.h"
#include <config_manager.h>
#include "../include/core_log.h"
#include <functional>

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

int main() {
    ConfigManager::GetInstance().LoadConfig();

    // 创建4个线程，每个线程运行一个模块
    std::thread t1(LaunchMqttMessageSender);
    std::thread t2(LaunchStm32MessageReceiver);
    std::thread t3(LaunchCvModel);
    std::thread t4(CoreThread);

    // 等待所有线程结束
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    LOG_OK("All modules finished.");
    return 0;
}