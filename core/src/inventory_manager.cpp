#include "../include/inventory_manager.h"
#include <algorithm>
#include <cmath>
#include <iostream>

InventoryManager::InventoryManager(int static_confirmation_required) {
    // 初始化三类水果的面团池
    category_pools_[FruitType::APPLE] = CategoryPool();
    category_pools_[FruitType::BANANA] = CategoryPool();
    category_pools_[FruitType::ORANGE] = CategoryPool();
    last_weight_evidence_[FruitType::APPLE] = false;
    last_weight_evidence_[FruitType::BANANA] = false;
    last_weight_evidence_[FruitType::ORANGE] = false;
    static_confirmation_required_ = static_confirmation_required;
}

std::string InventoryManager::GenerateUID(FruitType type, uint32_t timestamp) {
    return std::to_string(static_cast<int>(type)) + "_" + std::to_string(timestamp);
}

void InventoryManager::TranslateInputs(const StaticRecognitionResult& static_res,
                                       const DynamicRecognitionResult& dyn_res,
                                       const FrigeratorHistoryInfo& history,
                                       std::vector<CoreWeightEvent>& out_weights,
                                       std::vector<CoreDynamicEvent>& out_dyn,
                                       std::map<FruitType, std::vector<CoreStaticDetail>>& out_static)
{
    // weights: 使用 STM32 的 weightTimestamp/weight 阵列，基线使用 history.weightTimestamp 最后一项
    int sz = FRIGERATOR_HISTORY_INFO_SIZE;
    int32_t last_w = 0;
    // 尝试使用最后一个作为基线
    last_w = history.weight[sz - 1];
    for (int i = 0; i < sz; ++i) {
        uint32_t ts = history.weightTimestamp[i];
        if (ts == 0) continue;
        int16_t delta = static_cast<int16_t>(static_cast<int32_t>(history.weight[i]) - last_w);
        CoreWeightEvent ev{ts, delta, false};
        out_weights.push_back(ev);
        last_w = history.weight[i];
    }

    // dyn: 直接转
    for (uint8_t i = 0; i < dyn_res.fruitCount; ++i) {
        CoreDynamicEvent d{dyn_res.fruitInfoWithTimestamp[i].timestamp, dyn_res.fruitInfoWithTimestamp[i].fruitInfo.fruitType};
        out_dyn.push_back(d);
    }

    // static: 按类型聚合
    for (uint8_t i = 0; i < static_res.fruitCount; ++i) {
        CoreStaticDetail s{static_cast<FruitType>(static_res.fruits[i].fruitInfo), static_cast<FreshnessLevel>(static_res.fruits[i].freshness)};
        out_static[s.type].push_back(s);
    }
}

void InventoryManager::ProcessDynamicPhase(const std::vector<CoreWeightEvent>& weights,
                                           const std::vector<CoreDynamicEvent>& dyn_events)
{
    // 重置本次重量证据
    last_weight_evidence_[FruitType::APPLE] = false;
    last_weight_evidence_[FruitType::BANANA] = false;
    last_weight_evidence_[FruitType::ORANGE] = false;

    // 简单时间窗匹配
    for (const auto& d : dyn_events) {
        for (auto& w : const_cast<std::vector<CoreWeightEvent>&>(weights)) {
            if (w.used) continue;
            if (std::abs((int64_t)w.timestamp - (int64_t)d.timestamp) < 2000) {
                // 匹配成功
                auto &pool = category_pools_[d.type];
                if (w.delta_weight < 0) {
                    pool.count = std::max(0, pool.count - 1);
                    pool.total_weight += w.delta_weight; // delta_weight negative
                } else {
                    pool.count += 1;
                    pool.total_weight += w.delta_weight;
                    pool.is_trusted = true;
                    last_weight_evidence_[d.type] = true;
                }
                w.used = true;
                break;
            }
        }
    }
}

void InventoryManager::ProcessStaticPhase(const std::map<FruitType, std::vector<CoreStaticDetail>>& static_map)
{
    std::vector<FruitType> all_types = {FruitType::APPLE, FruitType::BANANA, FruitType::ORANGE};
    for (auto t : all_types) {
        int memory_count = category_pools_[t].count;
        int vision_count = static_map.count(t) ? static_cast<int>(static_map.at(t).size()) : 0;

        if (memory_count == vision_count) {
            category_pools_[t].consecutive_match++;
            if (category_pools_[t].consecutive_match >= static_confirmation_required_) {
                category_pools_[t].is_trusted = true;
            }
        } else {
            // 0 自愈
            if (vision_count == 0) {
                category_pools_[t].count = 0;
                category_pools_[t].total_weight = 0;
                category_pools_[t].is_trusted = true;
                category_pools_[t].consecutive_match = 0;
            } else {
                // 若有重量证据则直接同步
                if (last_weight_evidence_[t]) {
                    category_pools_[t].count = vision_count;
                    // total_weight 保持原值（被认为包含此次变化），但标记为不可信以便后续修正
                    category_pools_[t].is_trusted = false;
                    category_pools_[t].consecutive_match = 0;
                } else {
                    // 无重量证据：需要连续帧确认
                    if (category_pools_[t].consecutive_match > 0 && category_pools_[t].count == vision_count) {
                        category_pools_[t].consecutive_match++;
                    } else {
                        category_pools_[t].consecutive_match = 1;
                    }
                    if (category_pools_[t].consecutive_match >= static_confirmation_required_) {
                        category_pools_[t].count = vision_count;
                        category_pools_[t].is_trusted = false;
                        category_pools_[t].consecutive_match = 0;
                    } else {
                        category_pools_[t].is_trusted = false;
                    }
                }
            }
        }
    }
}

std::vector<CoreFinalFruit> InventoryManager::AssembleFinalInventory(const std::map<FruitType, std::vector<CoreStaticDetail>>& static_map)
{
    std::vector<CoreFinalFruit> out;
    for (const auto &pair : static_map) {
        FruitType t = pair.first;
        const auto &vec = pair.second;
        uint32_t per_weight = 999;
        if (category_pools_[t].is_trusted && vec.size() > 0) {
            per_weight = static_cast<uint32_t>(std::max(0, category_pools_[t].total_weight / static_cast<int>(vec.size())));
        }
        for (const auto &d : vec) {
            CoreFinalFruit f{t, d.freshness, per_weight};
            out.push_back(f);
        }
    }
    return out;
}

void InventoryManager::FormatToMqtt(const std::vector<CoreFinalFruit>& final_fruits, MqttMessageStruct& out_msg)
{
    out_msg.fruitCount = 0;
    for (size_t i = 0; i < final_fruits.size() && i < MAX_STATIC_FRUIT_COUNT; ++i) {
        const auto &f = final_fruits[i];
        out_msg.fruits[i].fruitInfo.fruitType = f.type;
        out_msg.fruits[i].fruitInfo.freshness = f.freshness;
        out_msg.fruits[i].weight = f.weight;
        out_msg.fruitCount++;
    }
}

StaticRecognitionResult InventoryManager::GenerateFinalStruct(uint32_t current_timestamp) {
    StaticRecognitionResult final_res;
    final_res.timestamp = current_timestamp;
    final_res.fruitCount = 0;
    return final_res;
}

uint32_t InventoryManager::GetFruitWeight(FruitType type) {
    // 返回面团池平均单果重（若可信）或默认值
    auto &pool = category_pools_[type];
    if (pool.is_trusted && pool.count > 0) {
        return static_cast<uint32_t>(std::max(0, pool.total_weight / pool.count));
    }
    switch (type) {
        case FruitType::APPLE: return 150;
        case FruitType::BANANA: return 120;
        case FruitType::ORANGE: return 130;
        default: return 100;
    }
}