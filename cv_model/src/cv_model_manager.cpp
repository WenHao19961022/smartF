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

bool CvModelManager::CvModelInit()
{
    std::atomic<bool> m_initFinished{false};
    std::atomic<bool> m_staticActive{false};
    std::atomic<bool> m_dynamicActive{false};

    m_dataMutex.lock();
    m_staticResult = {};
    m_dynamicResult = {};
    m_dataMutex.unlock();
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
    return m_staticResult;
}

DynamicRecognitionResult CvModelManager::GetDynamicResult()
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_dynamicResult;
}