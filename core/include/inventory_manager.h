#ifndef INVENTORY_MANAGER_H
#define INVENTORY_MANAGER_H

#include <vector>
#include <string>
#include <map>
#include <cstdint>
#include "./external_apis.h" // 包含水果类型等定义

struct TrackedFruit {
    uint16_t id;
    FruitType type;
    std::string uid;
    uint32_t putInTimestamp;
    uint8_t freshness;
    uint16_t locationX;
    uint16_t locationY;
};

struct CategoryStock {
    std::vector<TrackedFruit> fruits;
    int32_t totalWeight = 0;
};

class InventoryManager {
public:
    InventoryManager() = default;

    // V5.0 终极对账引擎接口
    void Reconcile(
        const std::map<FruitType, int32_t>& draftWeightDelta, 
        const std::map<FruitType, int32_t>& dynCountDelta, 
        const StaticRecognitionResult& statRes, 
        int32_t finalStableWeight, 
        uint32_t batchTs
    );

 
    // [新增] 用于定时静态检测，仅更新新鲜度和坐标，不影响数量和重量
    void UpdateStaticProperties(const StaticRecognitionResult& statRes);

    std::vector<TrackedFruit> GetFlattenedStock(std::map<FruitType, int32_t>& avgWeights);
    int32_t GetBookTotalWeight();
    uint16_t GetNextId() { return ++mNextFruitId; }

private:
    std::map<FruitType, CategoryStock> mStock;
    uint16_t mNextFruitId = 100;
};

#endif // INVENTORY_MANAGER_H