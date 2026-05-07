#include "inventory_manager.h"

#include <iostream>
#include <cstdlib>
#include <cmath>
#include <algorithm>

void InventoryManager::HandleDynamicEvent(FruitType type, int32_t weightDelta, int countDelta, uint32_t timestamp, int32_t realTotalWeight) {
    auto& category = mStock[type];

    // 更新重量
    category.totalWeight += weightDelta;

    // 更新数量占位符
    if (countDelta > 0) { // 放入
        for (int i = 0; i < countDelta; ++i) {
            TrackedFruit f;
            f.id = GetNextId();
            f.uid = std::to_string((int)type) + "_" + std::to_string(timestamp);
            f.type = type;
            f.putInTimestamp = timestamp;
            f.freshness = FreshnessLevel::Fresh; // 动态阶段未知时默认 Fresh，静态阶段会覆写
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
    int32_t bookTotal = GetBookTotalWeight();
    int32_t compensation = realTotalWeight - bookTotal;

    if (std::abs(compensation) > 10) {
        category.totalWeight += compensation;
    }

    // Debug Print
    std::cout << "[Dynamic] Type:" << (int)type << " | Delta: " << countDelta << "pcs, " << weightDelta << "g"
              << " | Comp: " << compensation << "g" << std::endl;
    std::cout << "[Dynamic] Snapshot: Type:" << (int)type << " | Cnt=" << category.fruits.size()
              << " | Weight=" << category.totalWeight << "g" << std::endl;
}

void InventoryManager::HandleStaticEvent(const StaticRecognitionResult& staticRes, uint32_t doorOpenTs) {
    // 1. 建立静态扫描结果映射
    std::map<FruitType, std::vector<FruitInfo>> staticMap;
    for (uint8_t i = 0; i < staticRes.fruitCount; ++i) {
        staticMap[staticRes.fruits[i].fruitType].push_back(staticRes.fruits[i]);
    }

    // 修复：主动接纳静态发现的新品类
    for (auto const& [type, infos] : staticMap) {
        if (mStock.find(type) == mStock.end()) {
            mStock[type] = CategoryStock();
        }
    }

    // 2. 遍历账本进行对齐和清理
    for (auto it = mStock.begin(); it != mStock.end(); ) {
        FruitType t = it->first;
        auto& category = it->second;
        size_t staticCount = staticMap.count(t) ? staticMap[t].size() : 0;

        // 种类清理：账本空且静态也没有
        if (category.fruits.empty() && staticCount == 0) {
            it = mStock.erase(it);
            continue;
        }

        // 数量强制对齐
        if (staticCount > category.fruits.size()) {
            size_t diff = staticCount - category.fruits.size();
            for (size_t i = 0; i < diff; ++i) {
                TrackedFruit f;
                f.id = GetNextId();
                f.type = t;
                // 使用开门时间戳作为 UID 批次标识，保证与动态阶段一致
                f.uid = std::to_string((int)t) + "_" + std::to_string(doorOpenTs);
                f.putInTimestamp = doorOpenTs;
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

    // Debug Print
    std::cout << "[Static] === Current Inventory Snapshot ===" << std::endl;
    for (auto const& [type, stock] : mStock) {
        int32_t avgW = 0;
        if (!stock.fruits.empty()) avgW = stock.totalWeight / (int32_t)stock.fruits.size();
        std::cout << "[Static] Type:" << (int)type << " | Count:" << stock.fruits.size()
                  << " | TotalWeight:" << stock.totalWeight << "g" << " | Avg:" << avgW << "g/pc" << std::endl;
    }
    std::cout << "[Static] ==================================" << std::endl;
}

std::vector<TrackedFruit> InventoryManager::GetFlattenedStock(std::map<FruitType, int32_t>& avgWeights) {
    std::vector<TrackedFruit> result;
    for (auto& [type, stock] : mStock) {
        if (stock.fruits.empty()) continue;

        // 计算均重
        int32_t avg = stock.totalWeight / (int32_t)stock.fruits.size();
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

int32_t InventoryManager::GetBookTotalWeight() {
    int32_t total = 0;
    for (auto const& [type, stock] : mStock) {
        total += stock.totalWeight;
    }
    return total;
}