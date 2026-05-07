#include "../include/core_manager.h"
#include <iostream>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <atomic>
#include "../include/core_log.h"
#include <string>

// 初始化随机数种子
namespace {
    struct RandomSeedInitializer {
        RandomSeedInitializer() {
            srand(static_cast<unsigned int>(time(nullptr)));
        }
    } randomSeedInitializer;
}

// 全局消息序列号（用于构造 32-bit messageId 的低 16 位）
static std::atomic<uint16_t> gMsgCounter(0);

void CoreManager::Init() {
    LOG_START("初始化开始");
    FrigeratorHistoryInfo initialInfo = GetFrigeratorInfo();
    mLastDoorState = (initialInfo.doorStatus[kFridgeHistoryInfoSize - 1] == DoorStatus::DoorOpen);
    mLastStaticTime = std::chrono::steady_clock::now();
    LOG_WARN("等待 CV 模型就绪...");
    while (!IsCvModelReady()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    LOG_OK("CV 模型已就绪，Core 接管控制权！");
}

void CoreManager::Run() {
    LOG_START("进入主循环");
    while (mRunning) {
        // 实时获取底层硬件状态（以最新的一次为准）
        FrigeratorHistoryInfo currInfo = GetFrigeratorInfo();
        bool currentDoorState = (currInfo.doorStatus[kFridgeHistoryInfoSize - 1] == DoorStatus::DoorOpen);

        if (currentDoorState != mLastDoorState) {               // 门状态变化，触发对应事件
            LOG_INFO(std::string("门状态突变触发: ") + (currentDoorState ? "关->开" : "开->关"));
            if (currentDoorState == true) {                       // 开门事件
                // 记录开门瞬间的重量作为基准
                mBaseWeight = currInfo.weight[kFridgeHistoryInfoSize - 1];
                HandleDoorOpen();
            } else {
                HandleDoorClose();                                  // 关门事件
            }
            mLastDoorState = currentDoorState;
        }

        if (mIsStaticWaiting && IsStaticRecognitionIdle() && !currentDoorState) {
            ProcessStaticResultOnly();
        }

        if (!currentDoorState && !mIsStaticWaiting) {
            CheckTimers();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void CoreManager::HandleDoorOpen() {
    LOG_START("处理开门逻辑");
    mIsStaticWaiting = false;
    // 记录开门瞬间时间戳（秒）作为本次批次基准
    mDoorOpenTimestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count());
    mBaseWeight = GetFrigeratorInfo().weight[kFridgeHistoryInfoSize - 1];
    LOG_DATA(std::string("开门瞬间抓取基准重量: ") + std::to_string(mBaseWeight) + "g, 时间戳: " + std::to_string(mDoorOpenTimestamp));
    StartDynamicRecognition();
    LOG_OK("HandleDoorOpen() - 执行完毕");
}

void CoreManager::HandleDoorClose() {
    LOG_START("处理关门逻辑 (启动动态对账)");
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
    uint16_t currentWeight = history.weight[kFridgeHistoryInfoSize - 1];

    // 计算净重跳变（相对于开门时的基准）
    int32_t weightDelta = static_cast<int32_t>(currentWeight) - static_cast<int32_t>(mBaseWeight);

    // 动态对账：从动态 CV 结果推断交互的水果种类（一次只交互一种）
    if (dyn.fruitCount > 0) {
        FruitType interactedType = dyn.fruitInfoWithTimestamp[0].fruitInfo.fruitType;
        int countDelta = 0;
        if (weightDelta > 0) countDelta = dyn.fruitCount; // 放入
        else countDelta = -static_cast<int>(dyn.fruitCount); // 取出

        // 执行动态对账 (V3.0 逻辑 1) — 传入开门基准时间戳作为 UID 批次
        mInventoryManager.HandleDynamicEvent(interactedType, weightDelta, countDelta, mDoorOpenTimestamp, static_cast<int32_t>(currentWeight));
    } else {
        if (weightDelta != 0) {
            LOG_WARN("Weight changed but CV saw no dynamic action!");
        }
    }

    // 执行静态对账 (V3.0 逻辑 2) — 传入开门基准时间戳用于 UID 生成
    mInventoryManager.HandleStaticEvent(stat, mDoorOpenTimestamp);

    // 生成 MQTT 报文 (V3.0 逻辑 3)
    std::map<FruitType, int32_t> avgWeights;
    std::vector<TrackedFruit> flattened = mInventoryManager.GetFlattenedStock(avgWeights);

    MqttMessageStruct msg;
    // 1. 生成时间戳和必填信息
    uint32_t timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());

    msg.time = timestamp;
    // 生成 messageId：高16位用 timestamp 低16位，低16位用原子序号
    uint16_t seq = gMsgCounter.fetch_add(1);
    msg.messageId = ((timestamp & 0xFFFFu) << 16) | seq;
    msg.deviceId = kFridgeDeviceId;

    // 填充底层硬件状态 (从 history 中获取)
    msg.fridgeInfo.temperature = history.temperature[kFridgeHistoryInfoSize - 1];
    msg.fridgeInfo.humidity = history.humidity[kFridgeHistoryInfoSize - 1];
    msg.fridgeInfo.doorStatus = DoorStatus::DoorClosed;
    msg.fridgeInfo.weight = history.weight[kFridgeHistoryInfoSize - 1];

    size_t fillCount = std::min((size_t)kMaxStaticFruitCount, flattened.size());
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

    // Debug Print -> log
    LOG_INFO(std::string("Sent MsgID: ") + std::to_string(msg.messageId) + " | Fruit Count: " + std::to_string((int)msg.fruitCount)
             + " | FridgeWeight: " + std::to_string(msg.fridgeInfo.weight) + "g");
    for (auto const& [t, w] : avgWeights) {
        LOG_INFO(std::string("Avg Weight Type ") + std::to_string((int)t) + ": " + std::to_string(w) + "g");
    }
    LOG_OK("动态对账流水写入并已发送 MQTT 报文");
}

void CoreManager::CheckTimers() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - mLastStaticTime) >= kStaticInterval) {
        LOG_START("触发2小时定时静态盘点");
        StartStaticRecognition();
        mIsStaticWaiting = true;
        mLastStaticTime = now;
        LOG_OK("定时拍照指令已下发");
    }
}

void CoreManager::ProcessStaticResultOnly() {
    LOG_START("收到静态照片，开始终极对账");
    mIsStaticWaiting = false;
    StaticRecognitionResult stat = GetStaticRecognitionResult();

    // 直接用静态结果做对账并上报（使用上一次开门时间戳作为批次UID）
    mInventoryManager.HandleStaticEvent(stat, mDoorOpenTimestamp);

    FrigeratorHistoryInfo currHistory = GetFrigeratorInfo();
    uint16_t currWeight = currHistory.weight[kFridgeHistoryInfoSize - 1];

    std::map<FruitType, int32_t> avgWeights;
    std::vector<TrackedFruit> flattened = mInventoryManager.GetFlattenedStock(avgWeights);

    MqttMessageStruct mqttMsg;
    mqttMsg.time = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
    uint32_t timestamp = mqttMsg.time;
    uint16_t seq2 = gMsgCounter.fetch_add(1);
    mqttMsg.messageId = ((timestamp & 0xFFFFu) << 16) | seq2;
    mqttMsg.deviceId = kFridgeDeviceId;

    mqttMsg.fridgeInfo.temperature = currHistory.temperature[kFridgeHistoryInfoSize - 1];
    mqttMsg.fridgeInfo.humidity = currHistory.humidity[kFridgeHistoryInfoSize - 1];
    mqttMsg.fridgeInfo.weight = currWeight;
    mqttMsg.fridgeInfo.doorStatus = DoorStatus::DoorClosed;

    size_t fillCount = std::min((size_t)kMaxStaticFruitCount, flattened.size());
    mqttMsg.fruitCount = static_cast<uint8_t>(fillCount);
    for (size_t i = 0; i < fillCount; ++i) {
        mqttMsg.fruits[i].id = flattened[i].id;
        mqttMsg.fruits[i].type = flattened[i].type;
        mqttMsg.fruits[i].freshness = flattened[i].freshness;
        int32_t avgW = 0;
        auto it = avgWeights.find(flattened[i].type);
        if (it != avgWeights.end()) avgW = it->second;
        mqttMsg.fruits[i].weight = (avgW > 0) ? static_cast<uint32_t>(avgW) : 0;
    }

    SendMqttMessage(mqttMsg);
    LOG_INFO(std::string("Sent MsgID: ") + std::to_string(mqttMsg.messageId) + " | Fruit Count: " + std::to_string((int)mqttMsg.fruitCount)
             + " | FridgeWeight: " + std::to_string(mqttMsg.fridgeInfo.weight) + "g");
}
