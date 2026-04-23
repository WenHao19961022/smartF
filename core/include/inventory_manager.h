#pragma once
#include <vector>
#include <map>
#include <string>
#include "external_apis.h"

// 内部追踪的业务实体，新增 weight 字段用于记录单体重量
struct TrackedFruit {
    std::string uid;           
    FruitType type;
    FreshnessLevel freshness;
    uint32_t put_in_timestamp; 
    int16_t estimated_weight; // 克 (g)
};

class InventoryManager {
private:
    std::map<FruitType, std::vector<TrackedFruit>> stock_;
    
    std::string GenerateUID(FruitType type, uint32_t timestamp);
    void UpdateFreshnessFromStatic(FruitType type, const StaticRecognitionResult& static_res);
    StaticRecognitionResult GenerateFinalStruct(uint32_t current_timestamp);

public:
    // 终极对账：时序对齐融合
    StaticRecognitionResult SettleInventory(const StaticRecognitionResult& static_res, 
                                            const DynamicRecognitionResult& dyn_res,
                                            const FrigeratorHistoryInfo& history,
                                            uint16_t base_weight);
};