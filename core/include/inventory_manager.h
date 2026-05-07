#ifndef INVENTORY_MANAGER_H
#define INVENTORY_MANAGER_H

#include <vector>
#include <string>
#include <map>
#include <cstdint>
#include <algorithm>
#include "./external_apis.h"

// ==================== 结构体 ====================
struct TrackedFruit {
    uint16_t id = 0;
    std::string uid;
    FruitType type;
    FreshnessLevel freshness;
    uint32_t putInTimestamp;
    uint8_t locationX;
    uint8_t locationY;
};

struct CategoryStock {
    int32_t totalWeight = 0;
    std::vector<TrackedFruit> fruits;
};

// ==================== 库存管理类 ====================
class InventoryManager {
public:
    InventoryManager() = default;

    void HandleDynamicEvent(FruitType type, int32_t weightDelta, int countDelta, uint32_t timestamp, int32_t realTotalWeight);
    void HandleStaticEvent(const StaticRecognitionResult& staticRes, uint32_t doorOpenTs);
    std::vector<TrackedFruit> GetFlattenedStock(std::map<FruitType, int32_t>& avgWeights);
    int32_t GetBookTotalWeight();
    uint16_t GetNextId() { return ++mNextFruitId; }

private:
    std::map<FruitType, CategoryStock> mStock;
    uint16_t mNextFruitId = 100;
};

#endif // INVENTORY_MANAGER_H