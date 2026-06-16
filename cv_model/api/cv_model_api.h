#ifndef CV_MODEL_API_H
#define CV_MODEL_API_H

#include <cstdint>

// ==================== 常量 ====================
const uint8_t kMaxStaticFruitCount = 10;
const uint8_t kMaxDynamicEventCount = 20;  // 单次开门最大变化事件数

// ==================== 枚举 ====================
enum class FruitType : uint8_t {
    Apple  = 1,
    Banana = 2,
    Orange = 3,
    Grape  = 4,
    Pear   = 5,
    Mango  = 6,
    PlasticBag = 7
};

enum class FreshnessLevel : uint8_t {
    Fresh  = 0,
    Stale  = 1,
    Rotten = 2
};

// 水果变化动作：放入 / 取出
enum class FruitChangeAction : uint8_t {
    PUT_IN   = 1,  // 放入冰箱
    TAKE_OUT = 2   // 从冰箱取出
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

// 单个水果变化事件
struct FruitChangeEvent {
    FruitType fruitType;
    FruitChangeAction action;  // 放入 / 取出
    uint32_t timestamp;        // 变化确认时刻
    uint8_t locationX;
    uint8_t locationY;
};

struct StaticRecognitionResult {
    uint32_t timestamp;
    uint8_t fruitCount;
    FruitInfo fruits[kMaxStaticFruitCount];
};

// 动态识别结果：变化事件列表
struct DynamicRecognitionResult {
    uint8_t eventCount;                           // 变化事件总数
    FruitChangeEvent events[kMaxDynamicEventCount]; // 按时间顺序排列的变化事件
};

// ==================== API函数 ====================
bool CvModelInit();
void CvModelMainLoop();
bool IsCvModelReady();
bool IsCameraReady();
void StartStaticRecognition();
void StartDynamicRecognition();
void StopStaticRecognition();
void StopDynamicRecognition();
bool IsStaticRecognitionIdle();
bool IsDynamicRecognitionIdle();
StaticRecognitionResult GetStaticRecognitionResult();
DynamicRecognitionResult GetDynamicRecognitionResult();

#endif // CV_MODEL_API_H
