#include "../api/cv_model_api.h"

bool cvModelReadyFlag = false; // cv_model准备就绪标志，cv_model在完成初始化后将该标志设置为true，core可以根据该标志判断cv_model是否准备就绪
bool cvModelStaticRecognitionFlag = false; // cv_model静态识别标志，core设置为true表示进行静态识别，cv_model识别完成后将该标志重置为false
bool cvModelDynamicRecognitionFlag = false; // cv_model动态识别标志，core设置为true表示进行动态识别，cv_model识别完成后将该标志重置为false

bool IsCvModelReady()
{
    return cvModelReadyFlag; // 返回cv_model准备就绪标志的值，core可以调用该函数检查cv_model的状态
}

void StartStaticRecognition()
{
    cvModelStaticRecognitionFlag = true; // 设置cv_model静态识别标志为true，core可以调用该函数启动静态识别
}

void StartDynamicRecognition()
{
    cvModelDynamicRecognitionFlag = true; // 设置cv_model动态识别标志为true，core可以调用该函数启动动态识别
}

bool IsStaticRecognitionComplete()
{
    return cvModelStaticRecognitionFlag; // 返回cv_model静态识别标志的值，core可以调用该函数检查静态识别的状态
}

bool IsDynamicRecognitionComplete()
{
    return cvModelDynamicRecognitionFlag; // 返回cv_model动态识别标志的值，core可以调用该函数检查动态识别的状态
}

// 读取静态识别结果的函数，core可以调用该函数获取最新的静态识别结果
StaticRecognitionResult GetStaticRecognitionResult()
{
    
}

// 读取动态识别结果的函数，core可以调用该函数获取最新的动态识别结果
DynamicRecognitionResult GetDynamicRecognitionResult()
{
    
}