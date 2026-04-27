#include "../include/core_manager.h"
#include <iostream>
#include <thread>

void CoreManager::init() {
    FrigeratorHistoryInfo initial_info = GetFrigeratorInfo();
    last_door_state_ = initial_info.historyInfo[4].doorStatus == 1;
    last_static_time_ = std::chrono::steady_clock::now();
    
    while (!IsCvModelReady()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "[Core] 就绪接管！" << std::endl;
}

void CoreManager::run() {
    while (running_) {
        // 实时获取底层硬件状态（以最新的一次为准）
        FrigeratorHistoryInfo curr_info = GetFrigeratorInfo();
        bool current_door_state = curr_info.historyInfo[4].doorStatus == 1;

        if (current_door_state != last_door_state_) {               // 门状态变化，触发对应事件
            if (current_door_state == true) {                       // 开门事件
                // 记录开门瞬间的重量作为基准
                base_weight_ = curr_info.historyInfo[4].weight; 
                HandleDoorOpen();
            } else {
                HandleDoorClose();                                  // 关门事件
            }
            last_door_state_ = current_door_state;
        }

        if (is_static_waiting_ && IsStaticRecognitionComplete() && !current_door_state) {
            ProcessStaticResultOnly();
        }

        if (!current_door_state && !is_static_waiting_) {
            CheckTimers();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void CoreManager::HandleDoorOpen() {
    is_static_waiting_ = false; 
    StartDynamicRecognition();
}

void CoreManager::HandleDoorClose() {
    StopDynamicRecognition(); 
    
    while (!IsDynamicRecognitionComplete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    DynamicRecognitionResult dyn_res = GetDynamicRecognitionResult();

    StartStaticRecognition();
    while (!IsStaticRecognitionComplete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    StaticRecognitionResult static_res = GetStaticRecognitionResult();
    
    // 关门后获取完整的历史重量过程
    FrigeratorHistoryInfo history = GetFrigeratorInfo();

    // 核心调用：传入三模态数据进行融合修缮
    StaticRecognitionResult final_inventory = inventory_manager_.SettleInventory(
        static_res, dyn_res, history, base_weight_);

    MqttMessageStruct mqtt_msg;
    mqtt_msg.time = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
    mqtt_msg.messageId = ++message_id_counter_;   ///////需要重定义 类似时间+随机数
    mqtt_msg.fridgeId = FRIDGE_DEVICE_ID;
    
    // 最新硬件状态透传
    mqtt_msg.fridgeInfo = history.historyInfo[4]; 
    mqtt_msg.recognitionResult = final_inventory;

    SendMqttMessage(mqtt_msg);
    last_static_time_ = std::chrono::steady_clock::now();
}

void CoreManager::CheckTimers() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_static_time_) >= STATIC_INTERVAL) {
        StartStaticRecognition();
        is_static_waiting_ = true;
        last_static_time_ = now;
    }
}

void CoreManager::ProcessStaticResultOnly() {
    is_static_waiting_ = false;
    StaticRecognitionResult static_res = GetStaticRecognitionResult();
    DynamicRecognitionResult empty_dyn_res = {0}; 
    FrigeratorHistoryInfo empty_history = {0};

    StaticRecognitionResult final_inventory = inventory_manager_.SettleInventory(
        static_res, empty_dyn_res, empty_history, GetFrigeratorInfo().historyInfo[4].weight);
    
    MqttMessageStruct mqtt_msg;
    mqtt_msg.time = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
    mqtt_msg.messageId = ++message_id_counter_;
    mqtt_msg.fridgeId = FRIDGE_DEVICE_ID;
    mqtt_msg.fridgeInfo = GetFrigeratorInfo().historyInfo[4];
    mqtt_msg.finalResult = final_inventory;

    SendMqttMessage(mqtt_msg);
}