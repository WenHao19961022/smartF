#include "../include/core_manager.h"
#include <iostream>
#include <thread>

void CoreManager::init() {
    last_static_time_ = std::chrono::steady_clock::now();
    last_gate_state_ = 0; // 假设初始状态门是关的
    current_state_ = SystemState::IDLE;
    std::cout << "[Core] 统一 API 架构核心管理器初始化完成." << std::endl;
}

void CoreManager::run() {
    while (running_) {
        // 1. 纯 API 调用：从 STM32 获取最新门状态
        auto fridge_info = GetFrigeratorInfo();
        int current_gate_state = fridge_info.doorStatus;

        // 2. 检测状态跳变并切换状态机
        handleStateTransition(current_gate_state);

        // 3. 执行当前状态下该做的事
        processCurrentState();

        // 4. 检查是否有定时任务
        checkRoutineTimers();

        // 5. 必须休眠一段时间，防止 while(true) 跑满 Jetson 的 CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void CoreManager::handleStateTransition(int current_gate_state) {
    // 检测到开门 (上升沿)
    if (current_gate_state == 1 && last_gate_state_ == 0) {
        std::cout << "[Core] 发现门开，调用 API 启动动态检测..." << std::endl;
        StartDynamicRecognition(); // 调用 cv_model_api
        current_state_ = SystemState::DOOR_OPEN_DYNAMIC;
    }
    // 检测到关门 (下降沿)
    else if (current_gate_state == 0 && last_gate_state_ == 1) {
        std::cout << "[Core] 发现门关，调用 API 停止动态并启动静态检测..." << std::endl;
        StartStaticRecognition(); // 调用 cv_model_api
        current_state_ = SystemState::DOOR_CLOSED_STATIC;
        last_static_time_ = std::chrono::steady_clock::now(); // 重置定时器
    }
    
    last_gate_state_ = current_gate_state;
}

void CoreManager::processCurrentState() {
    switch (current_state_) {
        case SystemState::IDLE:
            // 待机状态，无需特别操作
            break;

        case SystemState::DOOR_OPEN_DYNAMIC:
            // 门开着，持续查询 CV 动态识别状态
            if (IsDynamicRecognitionComplete()) {
                std::cout << "[Core] 动态识别已完成，门仍然开着，将继续保持动态识别状态..." << std::endl;
                StartDynamicRecognition();
            }
            break;

        case SystemState::DOOR_CLOSED_STATIC:
            // 纯 API 轮询：不断查问 CV 静态识别做完没
            if (IsStaticRecognitionComplete()) {
                std::cout << "[Core] 静态扫描完成，打包数据上传 Server..." << std::endl;
                
                // 1. 获取视觉结果
                auto static_res = GetStaticRecognitionResult();
                
                // 2. 获取称重结果 (假设 stm32_api 提供了 GetWeight)
                // float weight = GetWeight(); 
                
                // 3. 调用 Server API 上传
                // UploadData(json_payload); 
                
                // 处理完毕，切回待机状态
                current_state_ = SystemState::IDLE;
            }
            break;
    }
}

void CoreManager::checkRoutineTimers() {
    if (current_state_ == SystemState::IDLE) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_static_time_) >= STATIC_INTERVAL) {
            std::cout << "[Core] 定时器触发，调用 API 启动例行静态检测." << std::endl;
            StartStaticRecognition();
            current_state_ = SystemState::DOOR_CLOSED_STATIC; // 切换状态等待结果
            last_static_time_ = now;
        }
    }
}