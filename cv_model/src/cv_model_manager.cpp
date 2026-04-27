#include "../include/cv_model_manager.h"

// 构造函数
CvModelManager::CvModelManager()
{
    CvModelInit();
}

// 析构函数
CvModelManager::~CvModelManager()
{
    CvModelInit();
}

void CvModelManager::CvModelInit()
{
    std::atomic<bool> m_initStatus{INITI_UNFINISHED};
    std::atomic<bool> m_staticRecognitionSwitch{RECOGNITION_SWITCH_OFF};
    std::atomic<bool> m_dynamicecognitionSwitch{RECOGNITION_SWITCH_OFF};
    std::atomic<bool> m_staticRecognitionStatus{RECOGNITION_IDLE};
    std::atomic<bool> m_dynamicecognitionStatus{RECOGNITION_IDLE};

    m_dataMutex.lock();
    m_staticRecognitionResult = {};
    m_dynamicRecognitionResult = {};
    m_dataMutex.unlock();

    CvModelReady();
    return;
}

CvModelManager& CvModelManager::GetInstance()
{
    static CvModelManager instance;
    return instance;
}

void CvModelManager::MainLoop()
{
    while (true)
    {
    }
}

void CvModelManager::StaticRecognitionInternal()
{ 
}

void CvModelManager::DynamicRecognitionInternal()
{ 
}

StaticRecognitionResult CvModelManager::GetStaticResult()
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_staticRecognitionResult;
}

DynamicRecognitionResult CvModelManager::GetDynamicResult()
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_dynamicRecognitionResult;
}