#include "../include/cv_model_manager.h"
#include "../api/cv_model_api.h"

bool IsCvModelReady()
{
    return CvModelManager::GetInstance().IsCvModelReady();
}

void StartStaticRecognition()
{
<<<<<<< HEAD
    return CvModelManager::GetInstance().SetStaticRecognitionSwitch(RECOGNITION_SWITCH_ON);
=======
    return CvModelManager::GetInstance().SetStaticRecognitionStatus(true);
>>>>>>> 89325f4... zbhFixStartDynamicRecognition
}

void StartDynamicRecognition()
{
<<<<<<< HEAD
    return CvModelManager::GetInstance().SetDynamicRecognitionSwitch(RECOGNITION_SWITCH_ON);
}

void StopStaticRecognition()
{
    return CvModelManager::GetInstance().SetStaticRecognitionSwitch(RECOGNITION_SWITCH_OFF);
}

void StopDynamicRecognition()
{
    return CvModelManager::GetInstance().SetDynamicRecognitionSwitch(RECOGNITION_SWITCH_OFF);
=======
    return CvModelManager::GetInstance().SetDynamicRecognitionStatus(true);
>>>>>>> 89325f4... zbhFixStartDynamicRecognition
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