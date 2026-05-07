#include "inventory_manager.h"

#include <iostream>
#include <cstdlib>
#include <cmath>
#include <algorithm>

void InventoryManager::handleDynamicEvent(FruitType type, int32_t weightDelta, int countDelta, uint32_t timestamp, int32_t realTotalWeight) {
    auto& category = stock_[type];
    
    // 更新重量
    category.total_weight += weightDelta;

    // 更新数量占位符
    if (countDelta > 0) { // 放入
        for (int i = 0; i < countDelta; ++i) {
            TrackedFruit f;
            f.uid = std::to_string((int)type) + "_" + std::to_string(timestamp);
            f.type = type;
            f.put_in_timestamp = timestamp;
            f.freshness = FreshnessLevel::FRESH; // 动态阶段未知时默认 FRESH，静态阶段会覆写
            category.fruits.push_back(f);
        }
    } else if (countDelta < 0) { // 取出
        int removeCount = std::abs(countDelta);
        int actualAvailable = category.fruits.size();
        int toRemove = std::min(removeCount, actualAvailable);
        // 从头部删除（FIFO），防御式编程：确保 toRemove>0
        if (toRemove > 0) {
            category.fruits.erase(category.fruits.begin(), category.fruits.begin() + toRemove);
        }
    }

    // 重量补偿校验
    int32_t bookTotal = getBookTotalWeight();
    int32_t compensation = realTotalWeight - bookTotal;
    
    if (std::abs(compensation) > 10) {
        category.total_weight += compensation;
    }

    // Debug Print 1 (生产格式)
    std::cout << "[Dynamic] Type:" << (int)type << " | Delta: " << countDelta << "pcs, " << weightDelta << "g"
              << " | Comp: " << compensation << "g" << std::endl;
    std::cout << "[Dynamic] Snapshot: Type:" << (int)type << " | Cnt=" << category.fruits.size()
              << " | Weight=" << category.total_weight << "g" << std::endl;
}

void InventoryManager::handleStaticEvent(const StaticRecognitionResult& static_res, uint32_t door_open_ts) {
    // 1. 建立静态扫描结果映射
    std::map<FruitType, std::vector<FruitInfo>> staticMap;
    for (uint8_t i = 0; i < static_res.fruitCount; ++i) {
        staticMap[static_res.fruits[i].fruitType].push_back(static_res.fruits[i]);
    }

    // 修复：主动接纳静态发现的新品类
    for (auto const& [type, infos] : staticMap) {
        if (stock_.find(type) == stock_.end()) {
            stock_[type] = CategoryStock();
        }
    }

    // 2. 遍历账本进行对齐和清理
    for (auto it = stock_.begin(); it != stock_.end(); ) {
        FruitType t = it->first;
        auto& category = it->second;
        size_t staticCount = staticMap.count(t) ? staticMap[t].size() : 0;

        // 种类清理：账本空且静态也没有
        if (category.fruits.empty() && staticCount == 0) {
            it = stock_.erase(it);
            continue;
        }

        // 数量强制对齐
        if (staticCount > category.fruits.size()) {
            size_t diff = staticCount - category.fruits.size();
            for (size_t i = 0; i < diff; ++i) {
                TrackedFruit f;
                f.type = t;
                // 使用开门时间戳作为 UID 批次标识，保证与动态阶段一致
                f.uid = std::to_string((int)t) + "_" + std::to_string(door_open_ts);
                f.put_in_timestamp = door_open_ts;
                category.fruits.push_back(f);
            }
        } else if (staticCount < category.fruits.size()) {
            size_t diff = category.fruits.size() - staticCount;
            category.fruits.erase(category.fruits.begin(), category.fruits.begin() + diff);
        }

        // 属性覆写 (附带边界保护)
        if (staticCount > 0) {
            auto& sInfos = staticMap[t];
            for (size_t i = 0; i < category.fruits.size(); ++i) {
                if (i < sInfos.size()) {
                    category.fruits[i].freshness = sInfos[i].freshness;
                    category.fruits[i].locationX = sInfos[i].locationX;
                    category.fruits[i].locationY = sInfos[i].locationY;
                }
            }
        }
        ++it;
    }

    // Debug Print 2 (生产格式)
    std::cout << "[Static] === Current Inventory Snapshot ===" << std::endl;
    for (auto const& [type, stock] : stock_) {
        int32_t avgW = 0;
        if (!stock.fruits.empty()) avgW = stock.total_weight / (int32_t)stock.fruits.size();
        std::cout << "[Static] Type:" << (int)type << " | Count:" << stock.fruits.size()
                  << " | TotalWeight:" << stock.total_weight << "g" << " | Avg:" << avgW << "g/pc" << std::endl;
    }
    std::cout << "[Static] ==================================" << std::endl;
}

std::vector<TrackedFruit> InventoryManager::getFlattenedStock(std::map<FruitType, int32_t>& avgWeights) {
    std::vector<TrackedFruit> result;
    for (auto& [type, stock] : stock_) {
        if (stock.fruits.empty()) continue;

        // 计算均重
        int32_t avg = stock.total_weight / (int32_t)stock.fruits.size();
        // 异常兜底：如果均重小于等于0，或异常巨大，标记为 999
        if (avg <= 0 || avg > 2000) {
            std::cout << "[Warning] Abnormal avg weight for Type " << (int)type
                      << ": " << avg << "g. Fallback to 999." << std::endl;
            avg = 999;
        }
        avgWeights[type] = avg;

        for (auto& f : stock.fruits) {
            result.push_back(f);
        }
    }
    return result;
}

int32_t InventoryManager::getBookTotalWeight() {
    int32_t total = 0;
    for (auto const& [type, stock] : stock_) {
        total += stock.total_weight;
    }
    return total;
}