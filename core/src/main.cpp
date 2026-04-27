#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include "../../cv_model/include/cv_model_manager.h"
#include "../include/core_manager.h"
#include "../../stm32_message_recever/api/stm32_message_recever_api.h"

static std::atomic<uint8_t> g_door_status{0};

FrigeratorInfo GetFrigeratorInfo()
{
    return FrigeratorInfo{0, 0, 0, g_door_status.load()};
}

// 模块函数声明
// ---------------------------------------------------------
// 1. 服务器模块线程
void ConnectServer() {
    std::cout << "ConnectServer running in thread " << std::this_thread::get_id() << std::endl;
    // 服务器连接线程具体逻辑
}

// 2. STM32 硬件模块线程
void ConnectStm32() {
    std::cout << "ConnectStm32 running in thread " << std::this_thread::get_id() << std::endl;

    auto last_toggle = std::chrono::steady_clock::now();
    bool door_open = false;
    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_toggle).count() >= 8) {
            door_open = !door_open;
            g_door_status.store(door_open ? 1 : 0);
            std::cout << "[Stm32] 模拟串口更新门状态: " << (door_open ? "OPEN" : "CLOSED") << std::endl;
            last_toggle = now;
        }

        // TODO: 这里可以替换成真实串口读取与解析逻辑
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
// 3. 视觉模块线程
void CvModel() {
    std::cout << "CvModel running in thread " << std::this_thread::get_id() << std::endl;
    // 视觉模块具体逻辑
    // 初始化视觉模型
    CvModelManager::GetInstance().CvModelInit();
    // 运行视觉主循环（处理摄像头推理）
    CvModelManager::GetInstance().MainLoop();
}

// 4. 核心调度模块线程
void Core() {
    std::cout << "Core running in thread " << std::this_thread::get_id() << std::endl;
    // 主线程具体逻辑
    CoreManager manager;
    manager.init();
    
    // 核心逻辑跑在你指定的 t4 线程里
    manager.run();
}

int main() {
    std::cout << "=== 智慧冰箱动态管理系统多线程架构启动 ===" << std::endl;
    // 创建4个线程，每个线程运行一个模块
    std::thread t1(ConnectServer);
    std::thread t2(ConnectStm32);
    std::thread t3(CvModel);
    std::thread t4(Core);

    // 等待所有线程结束
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    std::cout << "All modules finished." << std::endl;
    return 0;
}