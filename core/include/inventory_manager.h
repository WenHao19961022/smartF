#ifndef INVENTORY_MANAGER_H
#define INVENTORY_MANAGER_H

#include <vector>
#include <string>
#include <map>
#include <cstdint>
#include <algorithm>
#include "external_apis.h" // 包含 FruitType, FreshnessLevel 等定义

// 内部追踪的占位符（已剔除单体重量）
struct TrackedFruit {
    std::string uid;
    FruitType type;
    FreshnessLevel freshness;
    uint32_t put_in_timestamp;
    uint8_t locationX;
    uint8_t locationY;
};

// 品类库存结构
struct CategoryStock {
    int32_t total_weight = 0;             // 该类水果的总重量 (g)
    std::vector<TrackedFruit> fruits;     // 该类水果的实例数组，size() 即数量
};

class InventoryManager {
public:
    InventoryManager() = default;

    // 1. 动态对账：处理动作和重量补偿
    void handleDynamicEvent(FruitType type, int32_t weightDelta, int countDelta, uint32_t timestamp, int32_t realTotalWeight);

    // 2. 静态对账：数量强制对齐与属性覆写
    void handleStaticEvent(const StaticRecognitionResult& static_res, uint32_t door_open_ts);

    // 3. 最终输出：获取展平后的数据用于MQTT上报
    std::vector<TrackedFruit> getFlattenedStock(std::map<FruitType, int32_t>& avgWeights);

    // 辅助：获取当前账面总重
    int32_t getBookTotalWeight();

private:
    std::map<FruitType, CategoryStock> stock_;
};

#endif