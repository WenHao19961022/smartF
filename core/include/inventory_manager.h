#pragma once
#include <vector>
#include <map>
#include <string>
#include "./external_apis.h"

// 内部追踪的业务实体，新增 weight 字段用于记录单体重量
struct TrackedFruit {
    std::string uid;           
    FruitType type;
    FreshnessLevel freshness;
    uint32_t put_in_timestamp; 
    uint32_t estimated_weight; // 克 (g)
    uint8_t locationX;        // 水果在图像中的X坐标（用于精确匹配）
    uint8_t locationY;        // 水果在图像中的Y坐标（用于精确匹配）
};

// 专门用于向 Core 传递组装好数据的包装结构体
struct FinalInventory {
    uint8_t fruitCount;
    FruitInfoWithWeight fruits[MAX_STATIC_FRUIT_COUNT];
};

class InventoryManager {
private:
    std::map<FruitType, std::vector<TrackedFruit>> stock_;
    
    std::string GenerateUID(FruitType type, uint32_t timestamp);
    void UpdateFreshnessFromStatic(FruitType type, const StaticRecognitionResult& static_res);
    //StaticRecognitionResult GenerateFinalStruct(uint32_t current_timestamp);
    FinalInventory GenerateFinalStruct(); // 修改返回值

public:
    // 终极对账：时序对齐融合
    FinalInventory SettleInventory(const StaticRecognitionResult& static_res, 
                                            const DynamicRecognitionResult& dyn_res,
                                            const FrigeratorHistoryInfo& history,
                                            uint16_t base_weight);
};