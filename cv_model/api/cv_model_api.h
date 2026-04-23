#ifndef CV_MODEL_API_H
#define CV_MODEL_API_H

#include <cstdint> // 包含cstdint头文件以使用固定宽度整数类型

// 水果枚举
enum class FruitType : uint8_t {
    APPLE  = 0,
    BANANA = 1,
    ORANGE = 2,
    // 其他水果类型
};

// 水果新鲜度枚举
enum class FreshnessLevel : uint8_t {
    FRESH = 0,
    STALE = 1,
    ROTTEN = 2,
    // 其他新鲜度等级
};

// 水果识别信息结构体
struct FruitInfo {
    FruitType fruitType; // 水果的类型
    uint8_t locationX; // 水果在图像中的X坐标
    uint8_t locationY; // 水果在图像中的Y坐标
    FreshnessLevel freshness; // 水果的新鲜度，范围可以是
    // 其他水果相关的数据成员
};

// 静态识别数据传输结构体定义
struct StaticRecognitionResult {
    uint32_t timestamp; // 识别结果的时间戳
    uint8_t fruitCount; // 识别到的水果数量
    FruitInfo fruits[10]; // 识别到的水果信息数组，假设最多识别10个水果
};

// 动态识别数据传输结构体定义
struct DynamicRecognitionResult {
    uint8_t fruitCount; // 识别到的水果数量
    uint32_t timestamp[5]; // 识别结果的时间戳
    FruitInfo fruits[5]; // 识别到的水果信息数组，假设最多识别5个水果
    // 其他动态识别相关的数据成员
};

// cv_model 与 core通讯flag, 用于指示cv_model的状态，或告知cv_model执行某些操作
bool IsCvModelReady(); // 检查cv_model是否准备就绪，core可以调用该函数检查cv_model的状态
void StartStaticRecognition(); // 启动cv_model的静态识别，core可以调用该函数启动静态识别
void StartDynamicRecognition(); // 启动cv_model的动态识别，core可以调用该函数启动动态识别
void StopStaticRecognition(); // 停止cv_model的静态识别，core可以调用该函数停止静态识别
void StopDynamicRecognition(); // 停止cv_model的动态识别，core可以调用该函数停止动态识别
bool IsStaticRecognitionIdle(); // 检查cv_model静态识别状态
bool IsDynamicRecognitionIdle(); // 检查cv_model的动态识别状态

// 读取静态识别结果的函数，core可以调用该函数获取最新的静态识别结果
StaticRecognitionResult GetStaticRecognitionResult();
// 读取动态识别结果的函数，core可以调用该函数获取最新的动态识别结果
DynamicRecognitionResult GetDynamicRecognitionResult();

#endif // CV_MODEL_API_H