#pragma once
#include <chrono>
#include <atomic>

// 统一引入所有子模块的 API 头文件
#include "../../cv_model/api/cv_model_api.h"
#include "../../stm32_message_recever/api/stm32_message_recever_api.h"
// #include "../../server/api/server_api.h"       // 待实现

// 定义系统的宏观状态
enum class SystemState {
    IDLE,               // 待机状态（门关着，无事发生）
    DOOR_OPEN_DYNAMIC,  // 门开着，正在进行动态识别
    DOOR_CLOSED_STATIC  // 门刚关上，正在进行静态扫描处理
};

class CoreManager {
private:
    std::atomic<bool> running_{true};
    SystemState current_state_ = SystemState::IDLE;
    
    // 记录上一帧的门状态，用于检测上升沿/下降沿（0为关，1为开）
    int last_gate_state_ = 0; 
    
    // 定时扫描相关
    std::chrono::steady_clock::time_point last_static_time_;
    const std::chrono::seconds STATIC_INTERVAL{2 * 3600}; 

    void handleStateTransition(int current_gate_state);
    void processCurrentState();
    void checkRoutineTimers();

public:
    void init();
    void run();
    void stop() { running_ = false; }
};