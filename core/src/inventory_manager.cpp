#include "../include/inventory_manager.h"
#include <algorithm>
#include <cmath>
#include <iostream>

std::string InventoryManager::GenerateUID(FruitType type, uint32_t timestamp) {
    return std::to_string(static_cast<int>(type)) + "_" + std::to_string(timestamp);
}

StaticRecognitionResult InventoryManager::SettleInventory(
    const StaticRecognitionResult& static_res, 
    const DynamicRecognitionResult& dyn_res,
    const FrigeratorHistoryInfo& history,
    uint16_t base_weight) 
{
    // 1. 整理 STM32 重量变化事件轴
    // 计算每次记录相对于前一次（或初始重量）的差值
    struct WeightEvent { uint32_t ts; int16_t delta; bool used; };
    std::vector<WeightEvent> weight_events;
    uint16_t last_w = base_weight;
    
    for (int i = 0; i < 5; ++i) {
        if (history.timestamp[i] > 0) { // 过滤无效记录
            int16_t dw = static_cast<int16_t>(history.historyInfo[i].weight) - static_cast<int16_t>(last_w);
            if (std::abs(dw) > 10) { // 设定10g的噪声阈值
                weight_events.push_back({history.timestamp[i], dw, false});
            }
            last_w = history.historyInfo[i].weight;
        }
    }

    // 2. 时序对齐：遍历 CV 动态记录，寻找相近时间的重量变化
    for (uint8_t i = 0; i < dyn_res.fruitCount; ++i) {
        FruitType type = static_cast<FruitType>(dyn_res.fruits[i].fruitInfo);
        uint32_t cv_ts = dyn_res.timestamp[i];
        
        int16_t matched_weight = 0;
        int closest_idx = -1;
        uint32_t min_diff = 3; // 允许最大时间偏差 3 秒

        // 寻找最匹配的硬件重量事件
        for (size_t j = 0; j < weight_events.size(); ++j) {
            if (!weight_events[j].used) {
                uint32_t diff = (cv_ts > weight_events[j].ts) ? (cv_ts - weight_events[j].ts) : (weight_events[j].ts - cv_ts);
                if (diff <= min_diff) {
                    min_diff = diff;
                    closest_idx = j;
                }
            }
        }

        if (closest_idx != -1) {
            matched_weight = weight_events[closest_idx].delta;
            weight_events[closest_idx].used = true;
        }

        // 3. 执行确定的动作方向
        if (matched_weight > 0) {
            // 明确的放入动作
            TrackedFruit new_fruit;
            new_fruit.type = type;
            new_fruit.put_in_timestamp = cv_ts; 
            new_fruit.uid = GenerateUID(type, cv_ts);
            new_fruit.freshness = FreshnessLevel::FRESH; 
            new_fruit.estimated_weight = matched_weight; // 绑定单体重量！
            stock_[type].push_back(new_fruit);
            std::cout << "[Inventory] 放入检测: " << static_cast<int>(type) << " 重量: " << matched_weight << "g" << std::endl;
        } 
        else if (matched_weight < 0) {
            // 明确的取出动作，FIFO 剔除
            if (!stock_[type].empty()) {
                std::sort(stock_[type].begin(), stock_[type].end(), 
                    [](const TrackedFruit& a, const TrackedFruit& b) {
                        return a.put_in_timestamp < b.put_in_timestamp;
                    });
                stock_[type].erase(stock_[type].begin());
                std::cout << "[Inventory] 取出检测: " << static_cast<int>(type) << std::endl;
            }
        }
        // 如果 matched_weight == 0，说明没对齐，可能被当成遮挡噪声忽略
    }

    // 4. 静态对齐兜底（The Single Source of Truth）
    // 即便动态对齐漏抓了，依然以关门后的全景照片为准，强行补齐或删减库存
    std::map<FruitType, int> static_counts;
    for (uint8_t i = 0; i < static_res.fruitCount; ++i) {
        static_counts[static_cast<FruitType>(static_res.fruits[i].fruitInfo)]++;
        UpdateFreshnessFromStatic(static_cast<FruitType>(static_res.fruits[i].fruitInfo), static_res);
    }

    // 修复可能产生的静态数量与动态计算结果的偏差
    for (const auto& pair : static_counts) {
        FruitType type = pair.first;
        int target = pair.second;
        int current = stock_[type].size();
        
        while (current < target) {
            // 静态多，动态没抓到放入 -> 强行入库一个未知重量的水果
            TrackedFruit nf; nf.type = type; nf.put_in_timestamp = static_res.timestamp;
            nf.uid = GenerateUID(type, static_res.timestamp); nf.freshness = FreshnessLevel::FRESH; nf.estimated_weight = 0;
            stock_[type].push_back(nf);
            current++;
        }
        while (current > target) {
            // 静态少，动态没抓到拿出 -> 强行出库最老的水果
            std::sort(stock_[type].begin(), stock_[type].end(), [](const TrackedFruit& a, const TrackedFruit& b) { return a.put_in_timestamp < b.put_in_timestamp; });
            stock_[type].erase(stock_[type].begin());
            current--;
        }
    }

    return GenerateFinalStruct(static_res.timestamp);
}

void InventoryManager::UpdateFreshnessFromStatic(FruitType type, const StaticRecognitionResult& static_res) {
    // 逻辑与之前相同：收集状态并赋予给现存库存
    std::vector<FreshnessLevel> detected_freshness;
    for (uint8_t i = 0; i < static_res.fruitCount; ++i) {
        if (static_cast<FruitType>(static_res.fruits[i].fruitInfo) == type) {
            detected_freshness.push_back(static_cast<FreshnessLevel>(static_res.fruits[i].freshness));
        }
    }
    std::sort(stock_[type].begin(), stock_[type].end(), [](const TrackedFruit& a, const TrackedFruit& b) { return a.put_in_timestamp < b.put_in_timestamp; });
    std::sort(detected_freshness.begin(), detected_freshness.end(), std::greater<FreshnessLevel>());
    for (size_t i = 0; i < stock_[type].size() && i < detected_freshness.size(); ++i) {
        stock_[type][i].freshness = detected_freshness[i];
    }
}

StaticRecognitionResult InventoryManager::GenerateFinalStruct(uint32_t current_timestamp) {
    StaticRecognitionResult final_res;
    final_res.timestamp = current_timestamp;
    final_res.fruitCount = 0;
    for (const auto& pair : stock_) {
        for (const auto& fruit : pair.second) {
            if (final_res.fruitCount < 10) { 
                final_res.fruits[final_res.fruitCount].fruitInfo = static_cast<uint8_t>(fruit.type);
                final_res.fruits[final_res.fruitCount].freshness = static_cast<uint8_t>(fruit.freshness);
                final_res.fruits[final_res.fruitCount].locationX = 0; 
                final_res.fruits[final_res.fruitCount].locationY = 0;
                final_res.fruitCount++;
            }
        }
    }
    return final_res;
}