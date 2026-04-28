#pragma once
#include <vector>
#include <map>
#include <string>
#include "external_apis.h"

// 新架构：每个品类的宏观“面团池”
struct CategoryPool {
    int count = 0;             // 当前品类的总数量
    int32_t total_weight = 0;  // 当前品类的总重量 (克)
    bool is_trusted = true;    // 重量池是否可信
    int consecutive_match = 0; // 防抖计数器
};

// 核心干净事件/输出结构体（core 内部使用）
struct CoreWeightEvent {
    uint32_t timestamp;
    int16_t delta_weight; // 比如 -150 或 +160
    bool used = false;
};

struct CoreDynamicEvent {
    uint32_t timestamp;
    FruitType type;
};

struct CoreStaticDetail {
    FruitType type;
    FreshnessLevel freshness;
};

struct CoreFinalFruit {
    FruitType type;
    FreshnessLevel freshness;
    uint32_t weight; // 如果不可信，这里就是 999
};

class InventoryManager {
private:
    // 新的宏观账本
    std::map<FruitType, CategoryPool> category_pools_;

    std::string GenerateUID(FruitType type, uint32_t timestamp);
    // 适配器与流水线函数
    void TranslateInputs(const StaticRecognitionResult& static_res,
                         const DynamicRecognitionResult& dyn_res,
                         const FrigeratorHistoryInfo& history,
                         std::vector<CoreWeightEvent>& out_weights,
                         std::vector<CoreDynamicEvent>& out_dyn,
                         std::map<FruitType, std::vector<CoreStaticDetail>>& out_static);

    void ProcessDynamicPhase(const std::vector<CoreWeightEvent>& weights,
                             const std::vector<CoreDynamicEvent>& dyn_events);

    void ProcessStaticPhase(const std::map<FruitType, std::vector<CoreStaticDetail>>& static_map);

    std::vector<CoreFinalFruit> AssembleFinalInventory(const std::map<FruitType, std::vector<CoreStaticDetail>>& static_map);

    // 格式化到 MQTT 格式（把 CoreFinalFruit 填入 MqttMessageStruct）
    void FormatToMqtt(const std::vector<CoreFinalFruit>& final_fruits, MqttMessageStruct& out_msg);

    StaticRecognitionResult GenerateFinalStruct(uint32_t current_timestamp);

public:
    // static_confirmation_required: 连续几帧静态一致才强制同步
    InventoryManager(int static_confirmation_required = 2);

    // 终极对账：时序对齐融合
    StaticRecognitionResult SettleInventory(const StaticRecognitionResult& static_res, 
                                            const DynamicRecognitionResult& dyn_res,
                                            const FrigeratorHistoryInfo& history,
                                            uint16_t base_weight);
    // 获取信任状态，给 MQTT 组包使用
    bool IsWeightTrusted(FruitType type) { return category_pools_[type].is_trusted; }
    // 在信任状态下获取单果估重（克）
    uint32_t GetFruitWeight(FruitType type);

private:
    // 最近一次动态阶段是否为该类型提供重量证据
    std::map<FruitType, bool> last_weight_evidence_;
    // 连续静态确认帧数统计，用于静态兜底策略（保留备用字段）
    std::map<FruitType, int> consecutive_static_confirmations_;
    std::map<FruitType, int> last_static_counts_;
    int static_confirmation_required_ = 2;
};