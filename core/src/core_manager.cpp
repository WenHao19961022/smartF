#include "../include/core_manager.h"
#include "../include/core_log.h"
#include <algorithm>
#include <atomic>
#include <thread>

static std::atomic<uint16_t> gMsgCounter{0};

static void WaitForStaticBusy() {
    while (IsStaticRecognitionIdle()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

static void WaitForStaticIdle() {
    while (!IsStaticRecognitionIdle()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

static void WaitForDynamicBusy() {
    while (IsDynamicRecognitionIdle()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

static void WaitForDynamicIdle() {
    while (!IsDynamicRecognitionIdle()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void CoreManager::Init() {
    LOG_INFO("CoreManager 初始化成功");
}

uint32_t CoreManager::GetCurrentTimeMs() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

void CoreManager::Run() {
    LOG_START("进入主循环");
    int loopCount = 0;
    
    // 初始化定时器基准时间
    mLastStaticTime = std::chrono::steady_clock::now();

    while (mRunning) {
        loopCount++;

        // 实时获取底层硬件状态（以最新的一次为准）
        FrigeratorHistoryInfo currInfo = GetFrigeratorInfo();
        bool currentDoorState = (currInfo.doorStatus[kFridgeHistoryInfoSize - 1] == DoorStatus::DoorOpen);

        // [V5.0 逻辑] 门开期间：高频收集重量流
        if (currentDoorState) {
            uint32_t nowMs = GetCurrentTimeMs();
            uint16_t currWeight = currInfo.weight[kFridgeHistoryInfoSize - 1];
            if (mWeightStream.empty() || mWeightStream.back().weight != currWeight || (nowMs - mWeightStream.back().timestampMs > 50)) {
                mWeightStream.push_back({nowMs, currWeight});
            }
        }

        // [原始+V5.0逻辑] 门状态突变触发
        if (currentDoorState != mLastDoorState) {
            LOG_INFO(std::string("门状态突变触发: ") + (currentDoorState ? "关->开" : "开->关"));
            if (currentDoorState == true) {
                // 记录基准重量的逻辑已移入 HandleDoorOpen 以保证与流收集严格对齐
                HandleDoorOpen();
            } else {
                HandleDoorClose();
            }
            mLastDoorState = currentDoorState;
        }

        // 定时静态检测完成后的处理
        if (mIsStaticWaiting && IsStaticRecognitionIdle() && !currentDoorState) {
            ProcessStaticResultOnly();
        }

        // 触发定时静态检测
        if (!currentDoorState && !mIsStaticWaiting) {
            CheckTimers();
        }

        // [恢复原始逻辑] 每100次循环打印一次心跳日志
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
    
    // =========================================================================
    // 【极其关键的防并发锁】
    // 如果用户开门时，后台正好在跑定时静态检测，必须强制等待其结束！
    // 否则同时 StartDynamicRecognition 会导致模型冲突或显存 OOM。
    // =========================================================================
    if (!IsStaticRecognitionIdle()) {
        LOG_WARN("开门动作与定时检测冲突：正在强制结束静态检测...");
        StopStaticRecognition();
        WaitForStaticIdle();
    }
    // 无论如何，既然开门了，就放弃那次定时的后续处理逻辑，以这次开门的动态对账为准
    mIsStaticWaiting = false; 

    // 生成批次UID时间戳
    mDoorOpenTimestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count());
    
    // 清空旧流，压入绝对物理基座
    mWeightStream.clear();
    mBaseWeight = GetFrigeratorInfo().weight[kFridgeHistoryInfoSize - 1];
    mWeightStream.push_back({GetCurrentTimeMs(), mBaseWeight});
    
    LOG_DATA(std::string("开门瞬间抓取基座: ") + std::to_string(mBaseWeight) + "g");
    
    // 此时静态必定Idle，可以安全启动动态
    StartDynamicRecognition();
    WaitForDynamicBusy();
    LOG_INFO("动态识别已启动并进入忙碌状态");
}

// V5.0: 滑动窗口算法，抵抗手扶隔板的高频噪声
int32_t CoreManager::CalculateLocalDeltaW(uint32_t startTsMs, uint32_t endTsMs) {
    if (mWeightStream.empty()) return 0;
    const int kVarianceThreshold = 15; // 稳态允许的最大波动(15g)

    // 1. 向历史回溯，寻找动作发生前的稳态 (窗口大小: 3个采样点)
    uint16_t weightBefore = mBaseWeight;
    for (int i = static_cast<int>(mWeightStream.size()) - 3; i >= 0; --i) {
        if (mWeightStream[i+2].timestampMs <= startTsMs) {
            int w1 = mWeightStream[i].weight;
            int w2 = mWeightStream[i+1].weight;
            int w3 = mWeightStream[i+2].weight;
            if (std::max({w1, w2, w3}) - std::min({w1, w2, w3}) <= kVarianceThreshold) {
                weightBefore = (w1 + w2 + w3) / 3;
                break;
            }
        }
    }

    // 2. 向未来探索，寻找动作完成后的稳态
    uint16_t weightAfter = mWeightStream.back().weight;
    for (size_t i = 0; i + 2 < mWeightStream.size(); ++i) {
        if (mWeightStream[i].timestampMs >= endTsMs) {
            int w1 = mWeightStream[i].weight;
            int w2 = mWeightStream[i+1].weight;
            int w3 = mWeightStream[i+2].weight;
            if (std::max({w1, w2, w3}) - std::min({w1, w2, w3}) <= kVarianceThreshold) {
                weightAfter = (w1 + w2 + w3) / 3;
                break;
            }
        }
    }

    return static_cast<int32_t>(weightAfter) - static_cast<int32_t>(weightBefore);
}

void CoreManager::HandleDoorClose() {
    LOG_START("处理关门逻辑 (启动V5.0全链路对账)");
    auto handleStart = std::chrono::steady_clock::now();

    StopDynamicRecognition();
    WaitForDynamicIdle();
    DynamicRecognitionResult dyn = GetDynamicRecognitionResult();

    StartStaticRecognition();
    WaitForStaticBusy();
    WaitForStaticIdle();
    StaticRecognitionResult stat = GetStaticRecognitionResult();

    uint16_t finalStableWeight = GetFrigeratorInfo().weight[kFridgeHistoryInfoSize - 1];

    std::map<FruitType, int32_t> draftWeightDelta;
    std::map<FruitType, int32_t> dynCountDelta;

    // V5.0: 动作聚类引擎 (防止并发抓取的双重扣费)
    if (dyn.eventCount > 0) {
        std::vector<FruitChangeEvent> events(dyn.events, dyn.events + dyn.eventCount);
        std::sort(events.begin(), events.end(), [](const auto& a, const auto& b) {
            return a.timestamp < b.timestamp;
        });

        // 将间隔小于 800ms 的动作聚类为同一组
        std::vector<std::vector<FruitChangeEvent>> clusters;
        for (const auto& ev : events) {
            if (clusters.empty() || (ev.timestamp - clusters.back().back().timestamp > 800)) {
                clusters.push_back({ev});
            } else {
                clusters.back().push_back(ev);
            }
        }

        // 处理物理聚类
        for (const auto& cluster : clusters) {
            uint32_t startTs = cluster.front().timestamp;
            uint32_t endTs = cluster.back().timestamp;

            int32_t clusterDeltaW = CalculateLocalDeltaW(startTs, endTs);
            int direction = (clusterDeltaW > 5) ? 1 : ((clusterDeltaW < -5) ? -1 : 0);
            int32_t avgDeltaW = clusterDeltaW / static_cast<int32_t>(cluster.size());

            for (const auto& ev : cluster) {
                draftWeightDelta[ev.fruitType] += avgDeltaW;
                dynCountDelta[ev.fruitType] += direction;
            }
        }
    }

    // 调用 V5.0 终极对账大脑
    mInventoryManager.Reconcile(draftWeightDelta, dynCountDelta, stat, finalStableWeight, mDoorOpenTimestamp);

    // ================== MQTT 组装与上报 ==================
    std::map<FruitType, int32_t> avgWeights;
    std::vector<TrackedFruit> flatStock = mInventoryManager.GetFlattenedStock(avgWeights);
    
    MqttMessageStruct msg;
    // 生成时间戳和必填信息
    uint32_t timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());

    msg.time = timestamp;
    // 生成 messageId：高16位用 timestamp 低16位，低16位用原子序号
    uint16_t seq = gMsgCounter.fetch_add(1);
    msg.messageId = ((timestamp & 0xFFFFu) << 16) | seq;
    msg.deviceId = mDeviceId;

    // 获取当前冰箱状态
    FrigeratorHistoryInfo currFridgeState = GetFrigeratorInfo();
    msg.fridgeInfo.temperature = currFridgeState.temperature[kFridgeHistoryInfoSize - 1];
    msg.fridgeInfo.humidity = currFridgeState.humidity[kFridgeHistoryInfoSize - 1];
    msg.fridgeInfo.doorStatus = DoorStatus::DoorClosed;
    msg.fridgeInfo.weight = currFridgeState.weight[kFridgeHistoryInfoSize - 1];

    size_t fillCount = std::min((size_t)kMaxStaticFruitCount, flatStock.size());
    msg.fruitCount = static_cast<uint8_t>(fillCount);
    for (size_t i = 0; i < fillCount; ++i) {
        msg.fruits[i].id = flatStock[i].id;
        msg.fruits[i].type = flatStock[i].type;
        msg.fruits[i].freshness = static_cast<FreshnessLevel>(flatStock[i].freshness);
        int32_t avgW = 0;
        auto it = avgWeights.find(flatStock[i].type);
        if (it != avgWeights.end()) avgW = it->second;
        msg.fruits[i].weight = (avgW > 0) ? static_cast<uint32_t>(avgW) : 0;
    }

    LOG_DATA("MQTT msg built: msgId=" << msg.messageId
             << " | deviceId=" << msg.deviceId
             << " | fruitCount=" << (int)msg.fruitCount
             << " | temp=" << (msg.fridgeInfo.temperature / 10.0) << "C"
             << " | humidity=" << (msg.fridgeInfo.humidity / 10.0) << "%"
             << " | weight=" << msg.fridgeInfo.weight << "g");

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

    LOG_OK("关门结算流水线执行完毕并已发送 MQTT 上报");
}

void CoreManager::CheckTimers() {
    auto now = std::chrono::steady_clock::now();
    // 假设 mStaticInterval 在头文件中定义 (例如 std::chrono::seconds mStaticInterval{7200})
    if (now - mLastStaticTime > mStaticInterval) {
        LOG_INFO("定时器触发：启动静态检测更新新鲜度");

        // 先等待动态完全空闲，避免互斥冲突
        WaitForDynamicIdle();

        StartStaticRecognition();
        WaitForStaticBusy();
        mIsStaticWaiting = true;
        mLastStaticTime = now;
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
        mqttMsg.fruits[i].freshness = static_cast<FreshnessLevel>(flattened[i].freshness);
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
