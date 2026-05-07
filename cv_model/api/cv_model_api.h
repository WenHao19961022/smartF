#ifndef CV_MODEL_API_H
#define CV_MODEL_API_H

#include <cstdint>

// ==================== 常量 ====================
const uint8_t kMaxStaticFruitCount = 10;
const uint8_t kMaxDynamicFruitCount = 5;

// ==================== 枚举 ====================
enum class FruitType : uint8_t {
    Apple  = 1,
    Banana = 2,
    Orange = 3,
    Grape  = 4,
    Pear   = 5,
    Mango  = 6
};

enum class FreshnessLevel : uint8_t {
    Fresh  = 0,
    Stale  = 1,
    Rotten = 2
};

// ==================== 结构体 ====================
struct FruitInfo {
    FruitType fruitType;
    uint8_t locationX;
    uint8_t locationY;
    FreshnessLevel freshness;
};

struct FruitInfoWithWeight {
    FruitInfo fruitInfo;
    uint32_t weight;
};

struct FruitInfoWithTimestamp {
    uint32_t timestamp;
    FruitInfo fruitInfo;
};

struct StaticRecognitionResult {
    uint32_t timestamp;
    uint8_t fruitCount;
    FruitInfo fruits[kMaxStaticFruitCount];
};

struct DynamicRecognitionResult {
    uint8_t fruitCount;
    FruitInfoWithTimestamp fruitInfoWithTimestamp[kMaxDynamicFruitCount];
};

// ==================== API函数 ====================
bool CvModelInit();
void CvModelMainLoop();
bool IsCvModelReady();
void StartStaticRecognition();
void StartDynamicRecognition();
void StopStaticRecognition();
void StopDynamicRecognition();
bool IsStaticRecognitionIdle();
bool IsDynamicRecognitionIdle();
StaticRecognitionResult GetStaticRecognitionResult();
DynamicRecognitionResult GetDynamicRecognitionResult();

#endif // CV_MODEL_API_H
