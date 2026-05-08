#include "../include/core_manager.h"
#include <iostream>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <atomic>
#include "../include/core_log.h"
#include <config_manager.h>
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

    // 从配置管理器读取业务参数
    mDeviceId = static_cast<uint32_t>(ConfigManager::GetInstance().GetInt("device.id", 10001));
    mStaticInterval = std::chrono::seconds(ConfigManager::GetInstance().GetInt("inventory.static_interval_sec", 7200));

    LOG_INFO("Config: device_id=" << mDeviceId << " static_interval=" << mStaticInterval.count() << "s");

    FrigeratorHistoryInfo initialInfo = GetFrigeratorInfo();
    mLastDoorState = (initialInfo.doorStatus[kFridgeHistoryInfoSize - 1] == DoorStatus::DoorOpen);
    LOG_INFO("Initial door state: " << (mLastDoorState ? "OPEN" : "CLOSED"));
    mLastStaticTime = std::chrono::steady_clock::now();
    LOG_WARN("等待 CV 模型就绪...");
    while (!IsCvModelReady()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    LOG_OK("CV 模型已就绪，Core 接管控制权！");
    LOG_INFO("Init complete | mDeviceId=" << mDeviceId << " | mStaticInterval=" << mStaticInterval.count() << "s");
}

void CoreManager::Run() {
    LOG_START("进入主循环");
    int loopCount = 0;
    while (mRunning) {
        loopCount++;

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

        // 每100次循环打印一次心跳日志
        if (loopCount % 100 == 0) {
            LOG_INFO("[Core Run] heartbeat #" << loopCount
                     << " | door=" << (currentDoorState ? "OPEN" : "CLOSED")
                     << " | temp=" << (currInfo.temperature[kFridgeHistoryInfoSize - 1] / 10.0) << "C"
                     << " | humidity=" << (currInfo.humidity[kFridgeHistoryInfoSize - 1] / 10.0) << "%"
                     << " | weight=" << currInfo.weight[kFridgeHistoryInfoSize - 1] << "g"
                     << " | staticWaiting=" << mIsStaticWaiting
                     << " | baseWeight=" << mBaseWeight << "g");
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
    auto handleStart = std::chrono::steady_clock::now();
    const int kMaxWaitMs = 30000; // 最大等待30秒

    LOG_INFO("Step 1: StopDynamicRecognition");
    StopDynamicRecognition();

    LOG_INFO("Step 2: Waiting for dynamic recognition to complete...");
    int waitCount = 0;
    while (!IsDynamicRecognitionIdle()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        waitCount++;
        if (waitCount % 100 == 0) {
            LOG_WARN("Waiting for dynamic recognition... (" << (waitCount * 10) << "ms)");
        }
        // 超时检测：防止永久阻塞
        if (waitCount * 10 >= kMaxWaitMs) {
            LOG_ERR("Dynamic recognition timeout after " << kMaxWaitMs << "ms! Proceeding with empty result.");
            break;
        }
    }
    LOG_INFO("Dynamic recognition completed (waited " << (waitCount * 10) << "ms)");

    DynamicRecognitionResult dyn = GetDynamicRecognitionResult();
    LOG_DATA("Dynamic result: fruitCount=" << (int)dyn.fruitCount);
    for (uint8_t i = 0; i < dyn.fruitCount; ++i) {
        LOG_DATA("  DynFruit[" << (int)i << "]: type=" << (int)dyn.fruitInfoWithTimestamp[i].fruitInfo.fruitType
                 << " | pos=(" << (int)dyn.fruitInfoWithTimestamp[i].fruitInfo.locationX
                 << "," << (int)dyn.fruitInfoWithTimestamp[i].fruitInfo.locationY << ")"
                 << " | ts=" << dyn.fruitInfoWithTimestamp[i].timestamp);
    }

    LOG_INFO("Step 3: StartStaticRecognition");
    StartStaticRecognition();

    LOG_INFO("Step 4: Waiting for static recognition to complete...");
    waitCount = 0;
    while (!IsStaticRecognitionIdle()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        waitCount++;
        if (waitCount % 100 == 0) {
            LOG_WARN("Waiting for static recognition... (" << (waitCount * 10) << "ms)");
        }
        // 超时检测：静态识别需要多次拍照，给更多时间
        if (waitCount * 10 >= kMaxWaitMs) {
            LOG_ERR("Static recognition timeout after " << kMaxWaitMs << "ms! Proceeding with current result.");
            break;
        }
    }
    LOG_INFO("Static recognition completed (waited " << (waitCount * 10) << "ms)");

    StaticRecognitionResult stat = GetStaticRecognitionResult();
    LOG_DATA("Static result: fruitCount=" << (int)stat.fruitCount << " | timestamp=" << stat.timestamp);
    for (uint8_t i = 0; i < stat.fruitCount; ++i) {
        LOG_DATA("  StatFruit[" << (int)i << "]: type=" << (int)stat.fruits[i].fruitType
                 << " | pos=(" << (int)stat.fruits[i].locationX << "," << (int)stat.fruits[i].locationY << ")"
                 << " | freshness=" << (int)stat.fruits[i].freshness);
    }

    // 关门后获取完整的历史重量过程
    FrigeratorHistoryInfo history = GetFrigeratorInfo();
    uint16_t currentWeight = history.weight[kFridgeHistoryInfoSize - 1];

    // 计算净重跳变（相对于开门时的基准）
    int32_t weightDelta = static_cast<int32_t>(currentWeight) - static_cast<int32_t>(mBaseWeight);
    LOG_DATA("Weight delta: currentWeight=" << currentWeight << "g - baseWeight=" << mBaseWeight << "g = " << weightDelta << "g");

    // 动态对账：从动态 CV 结果推断交互的水果种类（一次只交互一种）
    if (dyn.fruitCount > 0) {
        FruitType interactedType = dyn.fruitInfoWithTimestamp[0].fruitInfo.fruitType;
        int countDelta = 0;
        if (weightDelta > 0) countDelta = dyn.fruitCount; // 放入
        else countDelta = -static_cast<int>(dyn.fruitCount); // 取出

        LOG_INFO("Step 5: Dynamic reconciliation | type=" << (int)interactedType
                 << " | weightDelta=" << weightDelta << "g"
                 << " | countDelta=" << countDelta
                 << " | direction=" << (weightDelta > 0 ? "PUT_IN" : "TAKE_OUT"));

        // 执行动态对账 (V3.0 逻辑 1) — 传入开门基准时间戳作为 UID 批次
        mInventoryManager.HandleDynamicEvent(interactedType, weightDelta, countDelta, mDoorOpenTimestamp, static_cast<int32_t>(currentWeight));
    } else {
        LOG_WARN("Dynamic CV saw 0 fruits during door open");
        if (weightDelta != 0) {
            LOG_WARN("Weight changed (" << weightDelta << "g) but CV saw no dynamic action!");
        }
    }

    // 执行静态对账 (V3.0 逻辑 2) — 传入开门基准时间戳用于 UID 生成
    LOG_INFO("Step 6: Static reconciliation (doorOpenTs=" << mDoorOpenTimestamp << ")");
    mInventoryManager.HandleStaticEvent(stat, mDoorOpenTimestamp);

    // 生成 MQTT 报文 (V3.0 逻辑 3)
    LOG_INFO("Step 7: Building MQTT message");
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
    msg.deviceId = mDeviceId;

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

    LOG_DATA("MQTT msg built: msgId=" << msg.messageId
             << " | deviceId=" << msg.deviceId
             << " | fruitCount=" << (int)msg.fruitCount
             << " | temp=" << (msg.fridgeInfo.temperature / 10.0) << "C"
             << " | humidity=" << (msg.fridgeInfo.humidity / 10.0) << "%"
             << " | weight=" << msg.fridgeInfo.weight << "g");

    LOG_INFO("Step 8: Sending MQTT message...");
    bool sendResult = SendMqttMessage(msg);
    LOG_INFO("SendMqttMessage result: " << (sendResult ? "SUCCESS" : "FAILED"));

    auto handleEnd = std::chrono::steady_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(handleEnd - handleStart).count();

    // Debug Print -> log
    LOG_INFO(std::string("HandleDoorClose total time: ") + std::to_string(durationMs) + "ms"
             + " | MsgID: " + std::to_string(msg.messageId)
             + " | Fruit Count: " + std::to_string((int)msg.fruitCount)
             + " | FridgeWeight: " + std::to_string(msg.fridgeInfo.weight) + "g");
    for (auto const& [t, w] : avgWeights) {
        LOG_INFO(std::string("Avg Weight Type ") + std::to_string((int)t) + ": " + std::to_string(w) + "g");
    }
    LOG_OK("动态对账流水写入并已发送 MQTT 报文");
}

void CoreManager::CheckTimers() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - mLastStaticTime) >= mStaticInterval) {
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

    LOG_DATA("Static result: fruitCount=" << (int)stat.fruitCount << " | timestamp=" << stat.timestamp);
    for (uint8_t i = 0; i < stat.fruitCount; ++i) {
        LOG_DATA("  StatFruit[" << (int)i << "]: type=" << (int)stat.fruits[i].fruitType
                 << " | pos=(" << (int)stat.fruits[i].locationX << "," << (int)stat.fruits[i].locationY << ")"
                 << " | freshness=" << (int)stat.fruits[i].freshness);
    }

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
    mqttMsg.deviceId = mDeviceId;

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

    LOG_DATA("MQTT msg built (static timer): msgId=" << mqttMsg.messageId
             << " | deviceId=" << mqttMsg.deviceId
             << " | fruitCount=" << (int)mqttMsg.fruitCount
             << " | temp=" << (mqttMsg.fridgeInfo.temperature / 10.0) << "C"
             << " | humidity=" << (mqttMsg.fridgeInfo.humidity / 10.0) << "%"
             << " | weight=" << mqttMsg.fridgeInfo.weight << "g");

    bool sendResult = SendMqttMessage(mqttMsg);
    LOG_INFO(std::string("Sent MsgID: ") + std::to_string(mqttMsg.messageId)
             + " | Fruit Count: " + std::to_string((int)mqttMsg.fruitCount)
             + " | FridgeWeight: " + std::to_string(mqttMsg.fridgeInfo.weight) + "g"
             + " | SendResult: " + (sendResult ? "SUCCESS" : "FAILED"));
}
