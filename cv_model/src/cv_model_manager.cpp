#include "../include/cv_model_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

// 构造函数
CvModelManager::CvModelManager()
{
    // 构造时不执行可能失败的初始化
}

// 析构函数
CvModelManager::~CvModelManager()
{
    // unique_ptr 会自动处理 InferenceEngine 的释放
}

bool CvModelManager::CvModelInit()
{
    m_initStatus.store(INITI_UNFINISHED);
    
    // 初始化原子状态
    m_staticRecognitionSwitch.store(RECOGNITION_SWITCH_OFF);
    m_dynamicecognitionSwitch.store(RECOGNITION_SWITCH_OFF);
    m_staticRecognitionStatus.store(RECOGNITION_IDLE);
    m_dynamicecognitionStatus.store(RECOGNITION_IDLE);

    try {
        // 1. 实例化推理引擎 (路径需根据 Jetson 实际路径调整)
        // 这里可以根据需要加载不同的模型文件
        m_inferenceEngine = std::make_unique<InferenceEngine>("cv_model/yolov8/yolo12n.engine");

        // 2. 清空结果缓冲区
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_staticRecognitionResult = {};
            m_dynamicRecognitionResult = {};
        }

        // 3. 标记初始化完成
        CvModelReady();
        std::cout << "[CvModelManager] TensorRT Engine Initialized Successfully." << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[CvModelManager] Critical Error during Init: " << e.what() << std::endl;
        return false;
    }
    return false;
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
        if (!IsCvModelReady()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // 静态识别逻辑触发
        if (IsStaticRecognitionSwitchOn() && IsStaticRecognitionIdle()) {
            // 这里通常使用异步调用或直接在循环中根据频率控制
            StaticRecognitionInternal();
        }

        // 动态识别逻辑触发
        if (IsDynamicRecognitionSwitchOn() && IsDynamicRecognitionIdle()) {
            DynamicRecognitionInternal();
        }

        // 防止 CPU 占用过高
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void CvModelManager::StaticRecognitionInternal()
{ 
    SetStaticRecognitionStatus(RECOGNITION_BUSY);

    // 1. 获取图像 (此处应接入你的相机或图像流模块)
    cv::Mat frame; 
    // frame = CameraModule::GetInstance().GetLatestFrame();

    if (!frame.empty()) {
        std::vector<float> rawOutput;
        
        // 2. 调用封装好的推理引擎
        if (m_inferenceEngine && m_inferenceEngine->Infer(frame, rawOutput)) {
            
            // 3. 后处理 (解析 YOLO 格式输出)
            // 这里需要根据 YOLO12 的输出 Tensor 结构进行解析
            std::lock_guard<std::mutex> lock(m_dataMutex);
            
            // 示例：更新结果结构体
            // m_staticRecognitionResult.timestamp = ...
            // m_staticRecognitionResult.objects = PostProcess(rawOutput);
        }
    }

    SetStaticRecognitionStatus(RECOGNITION_IDLE);
}

void CvModelManager::DynamicRecognitionInternal()
{ 
    SetDynamicRecognitionStatus(RECOGNITION_BUSY);

    // 动态识别逻辑：通常涉及跨帧跟踪或更频繁的检测
    cv::Mat frame;
    if (!frame.empty() && m_inferenceEngine) {
        std::vector<float> rawOutput;
        m_inferenceEngine->Infer(frame, rawOutput);
        
        std::lock_guard<std::mutex> lock(m_dataMutex);
        // 更新 m_dynamicRecognitionResult
    }

    SetDynamicRecognitionStatus(RECOGNITION_IDLE);
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