#include "../include/cv_model_manager.h"
#include "../api/cv_model_api.h"

bool CvModelInit() {
    return CvModelManager::GetInstance().CvModelInit();
}

void CvModelMainLoop() {
    CvModelManager::GetInstance().MainLoop();
}

bool IsCvModelReady() {
    return CvModelManager::GetInstance().IsCvModelReady();
}

void StartStaticRecognition() {
    CvModelManager::GetInstance().SetStaticRecognitionSwitch(kRecognitionSwitchOn);
}

void StartDynamicRecognition() {
    CvModelManager::GetInstance().SetDynamicRecognitionSwitch(kRecognitionSwitchOn);
}

void StopStaticRecognition() {
    CvModelManager::GetInstance().SetStaticRecognitionSwitch(kRecognitionSwitchOff);
}

void StopDynamicRecognition() {
    CvModelManager::GetInstance().SetDynamicRecognitionSwitch(kRecognitionSwitchOff);
}

bool IsStaticRecognitionIdle() {
    return CvModelManager::GetInstance().IsStaticRecognitionIdle();
}

bool IsDynamicRecognitionIdle() {
    return CvModelManager::GetInstance().IsDynamicRecognitionIdle();
}

StaticRecognitionResult GetStaticRecognitionResult() {
    return CvModelManager::GetInstance().GetStaticResult();
}

DynamicRecognitionResult GetDynamicRecognitionResult() {
    return CvModelManager::GetInstance().GetDynamicResult();
}
