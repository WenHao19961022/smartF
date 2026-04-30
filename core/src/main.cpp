#include <iostream>
#include <thread>
#include <mutex>
#include "../include/external_apis.h"
#include "../include/core_manager.h"


// 全局互斥锁
std::mutex cout_mutex;


// 模块函数声明
void LaunchMqttMessageSender() {
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "MqttMessageSender running in thread " << std::this_thread::get_id() << std::endl;
    }
    MqttMessageSenderMainLoop();
}

void LaunchStm32MessageReceiver() {
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Stm32MessageReceiver running in thread " << std::this_thread::get_id() << std::endl;
    }
    Stm32MessageReceverMainLoop();
}

void LaunchCvModel() {
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "CvModel running in thread " << std::this_thread::get_id() << std::endl;
    }
    CvModelMainLoop();
}

void Core() {
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Core running in thread " << std::this_thread::get_id() << std::endl;
    }
    // 主线程具体逻辑
    CoreManager manager;
    manager.init();
    manager.run();
}

int main() {
    // 创建4个线程，每个线程运行一个模块
    std::thread t1(LaunchMqttMessageSender);
    std::thread t2(LaunchStm32MessageReceiver);
    std::thread t3(LaunchCvModel);
    std::thread t4(Core);

    // 等待所有线程结束
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    std::cout << "All modules finished." << std::endl;
    return 0;
}