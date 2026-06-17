#include "../include/inventory_manager.h"
#include "../include/core_log.h"
#include <cmath>
#include <limits>
#include <numeric>

namespace {

void ApplyStaticPropertiesToCategory(
    FruitType type,
    const StaticRecognitionResult& statRes,
    CategoryStock& category)
{
    if (category.fruits.empty()) {
        return;
    }

    std::vector<uint8_t> detectionIndices;
    for (uint8_t i = 0; i < statRes.fruitCount; ++i) {
        if (statRes.fruits[i].fruitType == type) {
            detectionIndices.push_back(i);
        }
    }
    if (detectionIndices.empty()) {
        return;
    }

    std::vector<bool> detectionUsed(detectionIndices.size(), false);
    std::vector<bool> fruitUpdated(category.fruits.size(), false);

    auto applyDetection = [&](size_t fruitIndex, size_t detectionSlot) {
        const auto& det = statRes.fruits[detectionIndices[detectionSlot]];
        category.fruits[fruitIndex].freshness = static_cast<uint8_t>(det.freshness);
        category.fruits[fruitIndex].locationX = det.locationX;
        category.fruits[fruitIndex].locationY = det.locationY;
        detectionUsed[detectionSlot] = true;
        fruitUpdated[fruitIndex] = true;
    };

    // Existing items are matched by nearest known coordinate, so multiple apples
    // keep their own freshness/position instead of being paired by vector order.
    for (size_t fruitIndex = 0; fruitIndex < category.fruits.size(); ++fruitIndex) {
        const auto& fruit = category.fruits[fruitIndex];
        bool hasKnownLocation = fruit.locationX != 0 || fruit.locationY != 0;
        if (!hasKnownLocation) {
            continue;
        }

        float bestDist = std::numeric_limits<float>::max();
        int bestSlot = -1;
        for (size_t slot = 0; slot < detectionIndices.size(); ++slot) {
            if (detectionUsed[slot]) {
                continue;
            }
            const auto& det = statRes.fruits[detectionIndices[slot]];
            float dx = static_cast<float>(fruit.locationX) - det.locationX;
            float dy = static_cast<float>(fruit.locationY) - det.locationY;
            float dist = dx * dx + dy * dy;
            if (dist < bestDist) {
                bestDist = dist;
                bestSlot = static_cast<int>(slot);
            }
        }

        if (bestSlot >= 0) {
            applyDetection(fruitIndex, static_cast<size_t>(bestSlot));
        }
    }

    // New items have no stable coordinate yet; assign remaining detections in
    // detection order so every visible fruit still receives a freshness value.
    for (size_t fruitIndex = 0; fruitIndex < category.fruits.size(); ++fruitIndex) {
        if (fruitUpdated[fruitIndex]) {
            continue;
        }
        for (size_t slot = 0; slot < detectionIndices.size(); ++slot) {
            if (!detectionUsed[slot]) {
                applyDetection(fruitIndex, slot);
                break;
            }
        }
    }
}

}

void InventoryManager::Reconcile(
    const std::map<FruitType, int32_t>& draftWeightDelta, 
    const std::map<FruitType, int32_t>& dynCountDelta, 
    const StaticRecognitionResult& statRes, 
    int32_t finalStableWeight, 
    uint32_t batchTs) 
{
    LOG_START("V5.0 终极对账大脑启动: 理论建模与底噪隔离");
    const int32_t kWeightEvidenceThreshold = 20; // STM32 稳定重量小于该变化时，优先认为是遮挡/视觉抖动

    // === 1. 静态事实确立与均重计算器 ===
    std::map<FruitType, int32_t> staticCounts;
    for (uint8_t i = 0; i < statRes.fruitCount; ++i) {
        staticCounts[statRes.fruits[i].fruitType]++;
    }

    std::map<FruitType, int32_t> realCountDelta;
    for (auto const& [type, category] : mStock) {
        int32_t sCount = staticCounts.count(type) ? staticCounts[type] : 0;
        realCountDelta[type] = sCount - static_cast<int32_t>(category.fruits.size());
    }
    for (auto const& [type, count] : staticCounts) {
        if (mStock.find(type) == mStock.end()) realCountDelta[type] = count;
    }

    std::map<FruitType, int32_t> effectiveDynCountDelta = dynCountDelta;
    int32_t physicalDelta = finalStableWeight - GetBookTotalWeight();
    bool noRemovalWeightEvidence = physicalDelta > -kWeightEvidenceThreshold;
    bool noPutInWeightEvidence = physicalDelta < kWeightEvidenceThreshold;

    // 遮挡修正：视觉少看见了水果，但 STM32 总重量没有对应下降时，不把它当作取出。
    for (auto& [type, delta] : realCountDelta) {
        if (delta < 0 && noRemovalWeightEvidence) {
            LOG_WARN("遮挡保护: 静态识别显示 type=" << (int)type << " 少了 " << std::abs(delta)
                     << " 个，但重量变化仅 " << physicalDelta << "g，保留库存数量");
            delta = 0;
        }
    }

    // 动态误报修正：动态说取出/放入，但重量方向没有支持时，降低其对对账的影响。
    for (auto& [type, delta] : effectiveDynCountDelta) {
        if (delta < 0 && noRemovalWeightEvidence) {
            LOG_WARN("遮挡保护: 动态识别疑似误报 TAKE_OUT type=" << (int)type
                     << "，重量变化=" << physicalDelta << "g，忽略该动态数量变化");
            delta = 0;
        } else if (delta > 0 && noPutInWeightEvidence) {
            LOG_WARN("重量保护: 动态识别疑似误报 PUT_IN type=" << (int)type
                     << "，重量变化=" << physicalDelta << "g，忽略该动态数量变化");
            delta = 0;
        }
    }

    std::map<FruitType, int32_t> historicalAvgWeights;
    auto getAvgWeight = [&](FruitType t) -> int32_t {
        if (historicalAvgWeights.count(t)) return historicalAvgWeights[t];
        int32_t avg = 150; 
        if (mStock.count(t) && !mStock[t].fruits.empty()) {
            avg = mStock[t].totalWeight / mStock[t].fruits.size();
        }
        historicalAvgWeights[t] = avg;
        return avg;
    };

    // === 2. 交叉确权与阵营划分 ===
    std::vector<FruitType> matchedTypes;
    std::vector<FruitType> mismatchedTypes;
    std::map<FruitType, bool> allActiveTypes;

    for (const auto& [t, d] : realCountDelta) if (d != 0) allActiveTypes[t] = true;
    for (const auto& [t, d] : effectiveDynCountDelta) if (d != 0) allActiveTypes[t] = true;

    for (const auto& [t, _] : allActiveTypes) {
        int32_t rDelta = realCountDelta.count(t) ? realCountDelta[t] : 0;
        int32_t dDelta = effectiveDynCountDelta.count(t) ? effectiveDynCountDelta.at(t) : 0;
        if (rDelta == dDelta) matchedTypes.push_back(t);
        else mismatchedTypes.push_back(t);
    }

    // === 3. V5.0 核心：四大场景数学推演体系 ===
    std::map<FruitType, int32_t> finalWeightDelta;

    // Pass 1: 完美匹配项 (确信种类实报实销)
    for (FruitType t : matchedTypes) {
        finalWeightDelta[t] = draftWeightDelta.count(t) ? draftWeightDelta.at(t) : 0;
    }

    // Pass 2: 差异嫌疑项 (强行按静态分配理论重量，解开一进一出死结)
    for (FruitType t : mismatchedTypes) {
        int32_t rDelta = realCountDelta.count(t) ? realCountDelta[t] : 0;
        finalWeightDelta[t] = rDelta * getAvgWeight(t); 
    }

    // Pass 3: 提取与隔离分配物理底噪 (完美对应您的 场景B/C/D 切分公式)
    int32_t expectedFinalWeight = GetBookTotalWeight();
    for (const auto& [t, dw] : finalWeightDelta) expectedFinalWeight += dw;
    int32_t globalNoise = finalStableWeight - expectedFinalWeight;

    // 收紧修正：只要有嫌疑数据，它们全权背锅误差；若全是好人，大家均摊传感器漂移。
    std::vector<FruitType> noiseAbsorbers = mismatchedTypes.empty() ? matchedTypes : mismatchedTypes;

    double expectedAbsTotal = 0; // 变动品类总期望
    for (FruitType t : noiseAbsorbers) {
        int32_t rDelta = realCountDelta.count(t) ? realCountDelta[t] : 0;
        expectedAbsTotal += std::abs(rDelta * getAvgWeight(t));
    }

    // 执行预期绝对值均摊切分
    if (expectedAbsTotal > 0 && globalNoise != 0) {
        int32_t distributed = 0;
        for (FruitType t : noiseAbsorbers) {
            int32_t rDelta = realCountDelta.count(t) ? realCountDelta[t] : 0;
            if (rDelta != 0) {
                // Comp = 重量差值 * (|Expected_Delta| / 变动品类总期望)
                int32_t share = static_cast<int32_t>(globalNoise * (std::abs(rDelta * getAvgWeight(t)) / expectedAbsTotal));
                finalWeightDelta[t] += share;
                distributed += share;
            }
        }
        // 补偿整型截断导致的末尾几克丢失
        if (globalNoise != distributed && !noiseAbsorbers.empty()) {
            finalWeightDelta[noiseAbsorbers.front()] += (globalNoise - distributed);
        }
    }

    // === 4. 质量守恒入库与残值防击穿闭环 ===
    int32_t residualPool = (expectedAbsTotal == 0) ? globalNoise : 0; // 防御逻辑：无变动但有纯误差，下放给残值池

    for (auto const& [type, delta] : realCountDelta) {
        if (mStock.find(type) == mStock.end()) mStock[type] = CategoryStock();
        auto& category = mStock[type];

        category.totalWeight += finalWeightDelta[type];

        // 绝对服从静态数量变动进行 FIFO 出入库 (对应场景C)
        if (delta > 0) {
            for (int i = 0; i < delta; ++i) {
                TrackedFruit f{};
                f.id = GetNextId();
                f.type = type;
                f.putInTimestamp = batchTs;
                f.freshness = static_cast<uint8_t>(FreshnessLevel::Fresh);
                f.uid = std::to_string((int)type) + "_" + std::to_string(batchTs) + "_" + std::to_string(f.id);
                category.fruits.push_back(f);
            }
        } else if (delta < 0) {
            int removeCount = std::abs(delta);
            if (static_cast<size_t>(removeCount) >= category.fruits.size()) category.fruits.clear();
            else category.fruits.erase(category.fruits.begin(), category.fruits.begin() + removeCount);
        }
    }

    // 清理残值、物理托底与静态属性覆写
    for (auto it = mStock.begin(); it != mStock.end(); ) {
        FruitType type = it->first;
        auto& category = it->second;
        size_t currentCount = category.fruits.size();

        if (currentCount == 0) {
            residualPool += category.totalWeight; 
            category.totalWeight = 0;
            it = mStock.erase(it);
            continue;
        }

        // 防击穿托底
        int32_t minWeight = static_cast<int32_t>(currentCount) * 5;
        if (category.totalWeight < minWeight) {
            residualPool -= (minWeight - category.totalWeight);
            category.totalWeight = minWeight;
        }

        ApplyStaticPropertiesToCategory(type, statRes, category);
        ++it;
    }

    // 全库存强制平摊防御逻辑 (防击穿残值 或 存活期纯漂移)
    if (residualPool != 0 && !mStock.empty()) {
        size_t totalLiveFruits = 0;
        for (auto const& [t, s] : mStock) totalLiveFruits += s.fruits.size();

        if (totalLiveFruits > 0) {
            int32_t residualDistributed = 0;
            size_t processedFruits = 0;
            for (auto& [t, s] : mStock) {
                processedFruits += s.fruits.size();
                int32_t share = (processedFruits == totalLiveFruits) 
                                ? (residualPool - residualDistributed) 
                                : static_cast<int32_t>(residualPool * (static_cast<double>(s.fruits.size()) / totalLiveFruits));
                s.totalWeight += share;
                residualDistributed += share;
            }
        }
    }

    LOG_OK("Reconcile V5.0 完成 | 最终物理总重守恒: " << finalStableWeight << "g");
}

// void InventoryManager::HandleStaticEvent(const StaticRecognitionResult& statRes, uint32_t batchTs) {
//     std::map<FruitType, int32_t> emptyDraftDelta;
//     std::map<FruitType, int32_t> emptyDynCountDelta;
//     int32_t finalStableWeight = GetBookTotalWeight();
//     Reconcile(emptyDraftDelta, emptyDynCountDelta, statRes, finalStableWeight, batchTs);
// }

// [新增] 定时刷新属性的具体实现
void InventoryManager::UpdateStaticProperties(const StaticRecognitionResult& statRes) {
    LOG_INFO("执行定时刷新：对齐最新新鲜度与坐标");
    
    // 遍历现有账本，仅将最新的新鲜度和坐标覆盖上去
    for (auto& [type, category] : mStock) {
        ApplyStaticPropertiesToCategory(type, statRes, category);
    }
    LOG_OK("定时刷新完毕");
}

// ============== 辅助函数实现 (不需改变) ==============
int32_t InventoryManager::GetBookTotalWeight() {
    int32_t total = 0;
    for (const auto& [_, category] : mStock) total += category.totalWeight;
    return total;
}

std::vector<TrackedFruit> InventoryManager::GetFlattenedStock(std::map<FruitType, int32_t>& avgWeights) {
    std::vector<TrackedFruit> flatStock;
    avgWeights.clear();
    for (const auto& [type, category] : mStock) {
        if (!category.fruits.empty()) {
            avgWeights[type] = category.totalWeight / category.fruits.size();
            flatStock.insert(flatStock.end(), category.fruits.begin(), category.fruits.end());
        }
    }
    return flatStock;
}
