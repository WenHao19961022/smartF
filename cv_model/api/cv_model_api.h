#ifndef CV_MODEL_API_H
#define CV_MODEL_API_H

#include <cstdint> // 包含cstdint头文件以使用固定宽度整数类型

const static bool DETECT_ACTIVE = true; // 定义一个常量，表示检测是否激活
const static bool DETECT_INACTIVE = false; // 定义一个常量，表示检测是否不激活

// 水果枚举
enum class FruitType {
    APPLE = 0,
    BANANA = 1,
    ORANGE = 2,
    // 其他水果类型
};

// 水果新鲜度枚举
enum class FreshnessLevel {
    FRESH = 0,
    STALE = 1,
    ROTTEN = 2,
    // 其他新鲜度等级
};

// 水果识别信息结构体
struct FruitInfo {
    uint8_t fruitInfo; // 水果的类型
    uint8_t locationX; // 水果在图像中的X坐标
    uint8_t locationY; // 水果在图像中的Y坐标
    uint8_t freshness; // 水果的新鲜度，范围可以是
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
    uint32_t timestamp[4]; // 识别结果的时间戳
    FruitInfo fruits[4]; // 识别到的水果信息数组，假设最多识别4个水果
    // 其他动态识别相关的数据成员
};

// cv_model 与 core通讯flag, 用于指示cv_model的状态，或告知cv_model执行某些操作
bool IsCvModelReady(); // 检查cv_model是否准备就绪，core可以调用该函数检查cv_model的状态
void StartStaticRecognition(); // 启动cv_model的静态识别，core可以调用该函数启动静态识别
void StartDynamicRecognition(); // 启动cv_model的动态识别，core可以调用该函数启动动态识别
bool IsStaticRecognitionComplete(); // 检查cv_model的静态识别是否完成，core可以调用该函数检查静态识别的状态
bool IsDynamicRecognitionComplete(); // 检查cv_model的动态识别是否完成，core可以调用该函数检查动态识别的状态

// 读取静态识别结果的函数，core可以调用该函数获取最新的静态识别结果
StaticRecognitionResult GetStaticRecognitionResult();
// 读取动态识别结果的函数，core可以调用该函数获取最新的动态识别结果
DynamicRecognitionResult GetDynamicRecognitionResult();

#endif // CV_MODEL_API_H