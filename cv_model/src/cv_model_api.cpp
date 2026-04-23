#include "../api/cv_model_api.h"
#include "../include/cv_model_manager.h"

bool IsCvModelReady()
{
    return CvModelManager::GetInstance().IsCvModelReady();
}

void StartStaticRecognition()
{
    return CvModelManager::GetInstance().SetStaticRecognitionSwitch(DETECT_ACTIVE);
}

void StartDynamicRecognition()
{
    return CvModelManager::GetInstance().SetDynamicRecognitionSwitch(DETECT_ACTIVE);
}

void StopStaticRecognition()
{
    return CvModelManager::GetInstance().SetStaticRecognitionSwitch(DETECT_DEACTIVE);
}

void StopDynamicRecognition()
{
    return CvModelManager::GetInstance().SetDynamicRecognitionSwitch(DETECT_DEACTIVE);
}

bool IsStaticRecognitionIdle()
{
    return CvModelManager::GetInstance().IsStaticRecognitionIdle();
}

bool IsDynamicRecognitionIdle()
{
    return CvModelManager::GetInstance().IsDynamicRecognitionIdle();
}

// 读取静态识别结果的函数，core可以调用该函数获取最新的静态识别结果
StaticRecognitionResult GetStaticRecognitionResult()
{
    return CvModelManager::GetInstance().GetStaticResult();
}

// 读取动态识别结果的函数，core可以调用该函数获取最新的动态识别结果
DynamicRecognitionResult GetDynamicRecognitionResult()
{
    return CvModelManager::GetInstance().GetDynamicResult();
}