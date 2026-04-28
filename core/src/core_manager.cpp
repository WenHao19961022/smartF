#include "../include/core_manager.h"
#include <iostream>
#include <thread>
#include <cstdlib>
#include <ctime>

// 初始化随机数种子
namespace {
    struct RandomSeedInitializer {
        RandomSeedInitializer() {
            srand(static_cast<unsigned int>(time(nullptr)));
        }
    } randomSeedInitializer;
}

void CoreManager::init() {
    FrigeratorHistoryInfo initial_info = GetFrigeratorInfo();
    last_door_state_ = (initial_info.doorStatus[FRIGERATOR_HISTORY_INFO_SIZE - 1] == DoorStatus::DOOR_OPEN);
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
        bool current_door_state = (curr_info.doorStatus[FRIGERATOR_HISTORY_INFO_SIZE - 1] == DoorStatus::DOOR_OPEN);

        if (current_door_state != last_door_state_) {               // 门状态变化，触发对应事件
            if (current_door_state == true) {                       // 开门事件
                // 记录开门瞬间的重量作为基准
                base_weight_ = curr_info.weight[FRIGERATOR_HISTORY_INFO_SIZE - 1];
                HandleDoorOpen();
            } else {
                HandleDoorClose();                                  // 关门事件
            }
            last_door_state_ = current_door_state;
        }

        if (is_static_waiting_ && IsStaticRecognitionIdle() && !current_door_state) {
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
    
    while (!IsDynamicRecognitionIdle()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    DynamicRecognitionResult dyn_res = GetDynamicRecognitionResult();

    StartStaticRecognition();
    while (!IsStaticRecognitionIdle()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    StaticRecognitionResult static_res = GetStaticRecognitionResult();
    
    // 关门后获取完整的历史重量过程
    FrigeratorHistoryInfo history = GetFrigeratorInfo();

    // 核心调用：传入三模态数据进行融合修缮
    // 拿到携带单体重量的新版库存结构
    FinalInventory final_inventory = inventory_manager_.SettleInventory(
        static_res, dyn_res, history, base_weight_);

    MqttMessageStruct mqtt_msg;
    mqtt_msg.time = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
    // 生成时间戳+随机数格式的messageId
    uint32_t timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
    // 生成0-9999的随机数
    uint32_t random = rand() % 10000;
    // 组合时间戳和随机数，确保在uint32_t范围内
    mqtt_msg.messageId = (timestamp * 10000) + random;
    mqtt_msg.fridgeId = FRIDGE_DEVICE_ID;
    
    // 组装硬件信息
    FrigeratorInfo current_fridge_info;
    current_fridge_info.temperature = history.temperature[FRIGERATOR_HISTORY_INFO_SIZE - 1];
    current_fridge_info.humidity = history.humidity[FRIGERATOR_HISTORY_INFO_SIZE - 1];
    current_fridge_info.weight = history.weight[FRIGERATOR_HISTORY_INFO_SIZE - 1];
    current_fridge_info.doorStatus = DoorStatus::DOOR_CLOSED;

    // 最新硬件状态透传
    mqtt_msg.fridgeInfo = current_fridge_info; 

    // 适配新的 MQTT 水果数组填充
    mqtt_msg.fruitCount = final_inventory.fruitCount;
    for (uint8_t i = 0; i < final_inventory.fruitCount; ++i) {
        mqtt_msg.fruits[i] = final_inventory.fruits[i];
    }

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

    FrigeratorHistoryInfo curr_history = GetFrigeratorInfo();
    uint16_t curr_weight = curr_history.weight[FRIGERATOR_HISTORY_INFO_SIZE - 1];

    FinalInventory final_inventory = inventory_manager_.SettleInventory(
        static_res, empty_dyn_res, empty_history, curr_weight);
    
    MqttMessageStruct mqtt_msg;
    mqtt_msg.time = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());

    // 生成时间戳+随机数格式的messageId
    uint32_t timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
    // 生成0-9999的随机数
    uint32_t random = rand() % 10000;
    // 组合时间戳和随机数，确保在uint32_t范围内
    mqtt_msg.messageId = (timestamp * 10000) + random;
    mqtt_msg.fridgeId = FRIDGE_DEVICE_ID;
    
    FrigeratorInfo current_fridge_info;
    current_fridge_info.temperature = curr_history.temperature[FRIGERATOR_HISTORY_INFO_SIZE - 1];
    current_fridge_info.humidity = curr_history.humidity[FRIGERATOR_HISTORY_INFO_SIZE - 1];
    current_fridge_info.weight = curr_weight;
    current_fridge_info.doorStatus = DoorStatus::DOOR_CLOSED;

    mqtt_msg.fridgeInfo = current_fridge_info;
    
    // 数组拷贝
    mqtt_msg.fruitCount = final_inventory.fruitCount;
    for (uint8_t i = 0; i < final_inventory.fruitCount; ++i) {
        mqtt_msg.fruits[i] = final_inventory.fruits[i];
    }

    SendMqttMessage(mqtt_msg);
}