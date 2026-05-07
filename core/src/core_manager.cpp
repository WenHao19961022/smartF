#include "../include/core_manager.h"
#include <iostream>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <atomic>

// 初始化随机数种子
namespace {
    struct RandomSeedInitializer {
        RandomSeedInitializer() {
            srand(static_cast<unsigned int>(time(nullptr)));
        }
    } randomSeedInitializer;
}

// 全局消息序列号（用于构造 32-bit messageId 的低 16 位）
static std::atomic<uint16_t> g_msg_counter(0);

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
    // 记录开门瞬间时间戳（秒）作为本次批次基准
    door_open_timestamp_ = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count());
    StartDynamicRecognition();
}

void CoreManager::HandleDoorClose() {
    StopDynamicRecognition();
    
    while (!IsDynamicRecognitionIdle()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    DynamicRecognitionResult dyn = GetDynamicRecognitionResult();

    StartStaticRecognition();
    while (!IsStaticRecognitionIdle()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    StaticRecognitionResult stat = GetStaticRecognitionResult();

    // 关门后获取完整的历史重量过程
    FrigeratorHistoryInfo history = GetFrigeratorInfo();
    uint16_t current_weight = history.weight[FRIGERATOR_HISTORY_INFO_SIZE - 1];

    // 计算净重跳变（相对于开门时的基准）
    int32_t weightDelta = static_cast<int32_t>(current_weight) - static_cast<int32_t>(base_weight_);

    uint32_t now = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    // 动态对账：从动态 CV 结果推断交互的水果种类（一次只交互一种）
    if (dyn.fruitCount > 0) {
        FruitType interacted_type = dyn.fruitInfoWithTimestamp[0].fruitInfo.fruitType;
        int countDelta = 0;
        if (weightDelta > 0) countDelta = dyn.fruitCount; // 放入
        else countDelta = -static_cast<int>(dyn.fruitCount); // 取出

        // 执行动态对账 (V3.0 逻辑 1) — 传入开门基准时间戳作为 UID 批次
        inventory_manager_.handleDynamicEvent(interacted_type, weightDelta, countDelta, door_open_timestamp_, static_cast<int32_t>(current_weight));
    } else {
        if (weightDelta != 0) {
            std::cout << "[Warning] Weight changed but CV saw no dynamic action!" << std::endl;
        }
    }

    // 执行静态对账 (V3.0 逻辑 2) — 传入开门基准时间戳用于 UID 生成
    inventory_manager_.handleStaticEvent(stat, door_open_timestamp_);

    // 生成 MQTT 报文 (V3.0 逻辑 3)
    std::map<FruitType, int32_t> avgWeights;
    std::vector<TrackedFruit> flattened = inventory_manager_.getFlattenedStock(avgWeights);


    MqttMessageStruct msg;
    // 1. 生成时间戳和必填信息 (与 ProcessStaticResultOnly 保持一致)
    uint32_t timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
    uint32_t random_val = rand() % 10000;

    msg.time = timestamp;
    // 防止溢出：使用64位左移再异或随机数，最后截断为 uint32_t
    // 生成 messageId：高16位用 timestamp 低16位，低16位用原子序号，避免溢出且减少冲突概率
    uint16_t seq = g_msg_counter.fetch_add(1);
    msg.messageId = ((timestamp & 0xFFFFu) << 16) | seq;
    msg.deviceId = FRIDGE_DEVICE_ID;

    // 填充底层硬件状态 (从 history 中获取)
    msg.fridgeInfo.temperature = history.temperature[FRIGERATOR_HISTORY_INFO_SIZE - 1];
    msg.fridgeInfo.humidity = history.humidity[FRIGERATOR_HISTORY_INFO_SIZE - 1];
    msg.fridgeInfo.doorStatus = DoorStatus::DOOR_CLOSED;
    // 遵守外部 API：没有 totalWeight 字段，使用 fridgeInfo.weight
    msg.fridgeInfo.weight = history.weight[FRIGERATOR_HISTORY_INFO_SIZE - 1];

    size_t fillCount = std::min((size_t)MAX_STATIC_FRUIT_COUNT, flattened.size());
    msg.fruitCount = static_cast<uint8_t>(fillCount);
    for (size_t i = 0; i < fillCount; ++i) {
        msg.fruits[i].id = flattened[i].id;
        msg.fruits[i].type = flattened[i].type;
        msg.fruits[i].freshness = flattened[i].freshness;
        int32_t avgW = 0;
        auto it = avgWeights.find(flattened[i].type);
        if (it != avgWeights.end()) avgW = it->second;
        msg.fruits[i].weight = (avgW > 0) ? static_cast<uint32_t>(avgW) : 0;
    }

    SendMqttMessage(msg);

    // Debug Print 3 (生产格式)
    std::cout << "[MQTT] Sent MsgID: " << msg.messageId << " | Fruit Count: " << (int)msg.fruitCount
              << " | FridgeWeight: " << msg.fridgeInfo.weight << "g" << std::endl;
    for(auto const& [t, w] : avgWeights) {
        std::cout << "[MQTT] Avg Weight Type " << (int)t << ": " << w << "g" << std::endl;
    }
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
    StaticRecognitionResult stat = GetStaticRecognitionResult();

    // 直接用静态结果做对账并上报（使用上一次开门时间戳作为批次UID）
    inventory_manager_.handleStaticEvent(stat, door_open_timestamp_);

    FrigeratorHistoryInfo curr_history = GetFrigeratorInfo();
    uint16_t curr_weight = curr_history.weight[FRIGERATOR_HISTORY_INFO_SIZE - 1];

    std::map<FruitType, int32_t> avgWeights;
    std::vector<TrackedFruit> flattened = inventory_manager_.getFlattenedStock(avgWeights);

    MqttMessageStruct mqtt_msg;
    mqtt_msg.time = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
    uint32_t timestamp = mqtt_msg.time;
    uint32_t random = rand() % 10000;
    // ProcessStaticResultOnly: 生成 messageId 使用相同策略
    uint16_t seq2 = g_msg_counter.fetch_add(1);
    mqtt_msg.messageId = ((timestamp & 0xFFFFu) << 16) | seq2;
    mqtt_msg.deviceId = FRIDGE_DEVICE_ID;

    FrigeratorInfo current_fridge_info;
    current_fridge_info.temperature = curr_history.temperature[FRIGERATOR_HISTORY_INFO_SIZE - 1];
    current_fridge_info.humidity = curr_history.humidity[FRIGERATOR_HISTORY_INFO_SIZE - 1];
    current_fridge_info.weight = curr_weight;
    current_fridge_info.doorStatus = DoorStatus::DOOR_CLOSED;
    mqtt_msg.fridgeInfo = current_fridge_info;

    size_t fillCount = std::min((size_t)MAX_STATIC_FRUIT_COUNT, flattened.size());
    mqtt_msg.fruitCount = static_cast<uint8_t>(fillCount);
    for (size_t i = 0; i < fillCount; ++i) {
        mqtt_msg.fruits[i].id = flattened[i].id;
        mqtt_msg.fruits[i].type = flattened[i].type;
        mqtt_msg.fruits[i].freshness = flattened[i].freshness;
        int32_t avgW = 0;
        auto it = avgWeights.find(flattened[i].type);
        if (it != avgWeights.end()) avgW = it->second;
        mqtt_msg.fruits[i].weight = (avgW > 0) ? static_cast<uint32_t>(avgW) : 0;
    }

    SendMqttMessage(mqtt_msg);
    std::cout << "[MQTT] Sent MsgID: " << mqtt_msg.messageId << " | Fruit Count: " << (int)mqtt_msg.fruitCount
              << " | FridgeWeight: " << mqtt_msg.fridgeInfo.weight << "g" << std::endl;
}