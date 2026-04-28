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

    // 核心流水线：翻译输入 -> 动态盲记 -> 静态对账 -> 组装最终个体 -> 填充 MQTT
    std::vector<CoreWeightEvent> core_weights;
    std::vector<CoreDynamicEvent> core_dyn;
    std::map<FruitType, std::vector<CoreStaticDetail>> core_static_map;
    inventory_manager_.TranslateInputs(static_res, dyn_res, history, core_weights, core_dyn, core_static_map);
    inventory_manager_.ProcessDynamicPhase(core_weights, core_dyn);
    inventory_manager_.ProcessStaticPhase(core_static_map);
    std::vector<CoreFinalFruit> final_fruits = inventory_manager_.AssembleFinalInventory(core_static_map);

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

    // 最新硬件状态透传
    mqtt_msg.deviceId = FRIDGE_DEVICE_ID;
    mqtt_msg.fridgeInfo = history.historyInfo[4]; 

    // 将 final_fruits 填充到 mqtt_msg
    inventory_manager_.FormatToMqtt(final_fruits, mqtt_msg);

    // 尝试发送并在失败时重试/入队
    bool sent = SendMqttMessage(mqtt_msg);
    if (!sent) {
        int retry = 0;
        const int maxRetry = 2;
        while (!sent && retry < maxRetry) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            sent = SendMqttMessage(mqtt_msg);
            ++retry;
        }
        if (!sent) {
            EnqueueMqttMessage(mqtt_msg);
        }
    }

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

    // 翻译并执行流水线（无动态事件）
    std::vector<CoreWeightEvent> core_weights;
    std::vector<CoreDynamicEvent> core_dyn;
    std::map<FruitType, std::vector<CoreStaticDetail>> core_static_map;
    inventory_manager_.TranslateInputs(static_res, empty_dyn_res, empty_history, core_weights, core_dyn, core_static_map);
    inventory_manager_.ProcessDynamicPhase(core_weights, core_dyn);
    inventory_manager_.ProcessStaticPhase(core_static_map);
    std::vector<CoreFinalFruit> final_fruits = inventory_manager_.AssembleFinalInventory(core_static_map);

    MqttMessageStruct mqtt_msg;
    mqtt_msg.time = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
    uint32_t timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
    uint32_t random = rand() % 10000;
    mqtt_msg.messageId = (timestamp * 10000) + random;
    mqtt_msg.deviceId = FRIDGE_DEVICE_ID;
    mqtt_msg.fridgeInfo = GetFrigeratorInfo().historyInfo[4];

    inventory_manager_.FormatToMqtt(final_fruits, mqtt_msg);

    bool sent = SendMqttMessage(mqtt_msg);
    if (!sent) {
        int retry = 0;
        const int maxRetry = 2;
        while (!sent && retry < maxRetry) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            sent = SendMqttMessage(mqtt_msg);
            ++retry;
        }
        if (!sent) {
            EnqueueMqttMessage(mqtt_msg);
        }
    }
}
