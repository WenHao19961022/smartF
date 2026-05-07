#include "../include/cv_model_manager.h"
#include "../include/camera_model.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

CvModelManager& CvModelManager::GetInstance() {
    static CvModelManager instance;
    return instance;
}

CvModelManager::CvModelManager() {
}

CvModelManager::~CvModelManager() {
}

bool CvModelManager::CvModelInit() {
    m_initStatus.store(INITI_UNFINISHED);

    m_staticRecognitionSwitch.store(RECOGNITION_SWITCH_OFF);
    m_dynamicecognitionSwitch.store(RECOGNITION_SWITCH_OFF);
    m_staticRecognitionStatus.store(RECOGNITION_IDLE);
    m_dynamicecognitionStatus.store(RECOGNITION_IDLE);

    m_dataMutex.lock();
    m_staticRecognitionResult = {};
    m_dynamicRecognitionResult = {};
    m_dataMutex.unlock();

    try {
        m_inferenceEngine = std::make_unique<InferenceEngine>("cv_model/yolov8/yolo12n.engine");
        CvModelReady();
        std::cout << "[CvModelManager] TensorRT Engine Initialized Successfully." << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[CvModelManager] TensorRT init failed: " << e.what() << std::endl;
        std::cerr << "[CvModelManager] CV Model running in simulation mode." << std::endl;
        CvModelReady();
        return true;
    }
}

void CvModelManager::MainLoop() {
    while (true) {
        if (!IsCvModelReady()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        if (IsStaticRecognitionSwitchOn() && IsStaticRecognitionIdle()) {
            StaticRecognitionInternal();
        }

        if (IsDynamicRecognitionSwitchOn() && IsDynamicRecognitionIdle()) {
            DynamicRecognitionInternal();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// YOLO后处理：从原始TensorRT输出提取水果检测结果
// YOLOv8输出: [batch, 84, num_proposals] 其中84=4(box)+80(classes)
// 实际使用时需根据模型实际输出维度调整
static std::vector<FruitInfo> PostProcessYOLO(
    const std::vector<float>& output,
    const std::vector<int>& outputDims,
    float confThreshold = 0.5f,
    float nmsThreshold = 0.4f)
{
    std::vector<FruitInfo> results;

    if (output.empty() || outputDims.size() < 3) {
        return results;
    }

    int numProposals = outputDims[2];
    int numClasses = outputDims[1] - 4;  // 4个box参数 + N个类别

    const int NUM_FRUIT_CLASSES = 6;  // APPLE, BANANA, ORANGE, GRAPE, PEAR, MANGO

    struct Detection {
        float cx, cy, w, h;
        float confidence;
        int classId;
    };
    std::vector<Detection> detections;

    // 遍历所有proposals，提取检测结果
    for (int i = 0; i < numProposals; ++i) {
        float cx = output[i];
        float cy = output[numProposals + i];
        float bw = output[2 * numProposals + i];
        float bh = output[3 * numProposals + i];

        // 找最大类别置信度
        float maxConf = 0.0f;
        int maxClassId = 0;

        for (int c = 0; c < std::min(numClasses, NUM_FRUIT_CLASSES); ++c) {
            float conf = output[(4 + c) * numProposals + i];
            if (conf > maxConf) {
                maxConf = conf;
                maxClassId = c;
            }
        }

        if (maxConf > confThreshold) {
            detections.push_back({cx, cy, bw, bh, maxConf, maxClassId});
        }
    }

    // 简化的NMS
    std::vector<bool> suppressed(detections.size(), false);

    for (size_t i = 0; i < detections.size(); ++i) {
        if (suppressed[i]) continue;

        const auto& det = detections[i];
        FruitInfo info;
        info.fruitType = static_cast<FruitType>(det.classId + 1);
        info.locationX = static_cast<uint8_t>(std::min(255, static_cast<int>(det.cx)));
        info.locationY = static_cast<uint8_t>(std::min(255, static_cast<int>(det.cy)));
        info.freshness = FreshnessLevel::FRESH;  // 简化：默认新鲜，真实模型需输出新鲜度

        results.push_back(info);

        // 抑制同类重叠检测
        for (size_t j = i + 1; j < detections.size(); ++j) {
            if (suppressed[j] || detections[j].classId != det.classId) continue;

            float dx = det.cx - detections[j].cx;
            float dy = det.cy - detections[j].cy;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < nmsThreshold * std::max(det.w, det.h)) {
                suppressed[j] = true;
            }
        }
    }

    return results;
}

void CvModelManager::StaticRecognitionInternal() {
    SetStaticRecognitionStatus(RECOGNITION_BUSY);

    cv::Mat frame = CameraModule::GetInstance().GetLatestFrame();
    StaticRecognitionResult result = {};
    result.timestamp = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    if (!frame.empty() && m_inferenceEngine) {
        std::vector<float> rawOutput;
        if (m_inferenceEngine->Infer(frame, rawOutput)) {
            auto detections = PostProcessYOLO(rawOutput, m_inferenceEngine->GetOutputDims());

            result.fruitCount = std::min(static_cast<uint8_t>(detections.size()),
                                        MAX_STATIC_FRUIT_COUNT);
            for (uint8_t i = 0; i < result.fruitCount; ++i) {
                result.fruits[i] = detections[i];
            }

            std::cout << "[CvModel] Static: Detected " << (int)result.fruitCount << " fruits" << std::endl;
        }
    } else if (!frame.empty()) {
        // 模拟模式（无TensorRT引擎时）
        result.fruitCount = 2;
        result.fruits[0].fruitType = FruitType::ORANGE;
        result.fruits[0].locationX = 120;
        result.fruits[0].locationY = 100;
        result.fruits[0].freshness = FreshnessLevel::FRESH;

        result.fruits[1].fruitType = FruitType::BANANA;
        result.fruits[1].locationX = 200;
        result.fruits[1].locationY = 150;
        result.fruits[1].freshness = FreshnessLevel::STALE;

        std::cout << "[CvModel] Static (SIM): Detected " << (int)result.fruitCount << " fruits" << std::endl;
    }

    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_staticRecognitionResult = result;
    }

    SetStaticRecognitionStatus(RECOGNITION_IDLE);
}

void CvModelManager::DynamicRecognitionInternal() {
    SetDynamicRecognitionStatus(RECOGNITION_BUSY);

    cv::Mat frame = CameraModule::GetInstance().GetLatestFrame();
    DynamicRecognitionResult result = {};

    if (!frame.empty() && m_inferenceEngine) {
        std::vector<float> rawOutput;
        if (m_inferenceEngine->Infer(frame, rawOutput)) {
            auto detections = PostProcessYOLO(rawOutput, m_inferenceEngine->GetOutputDims());

            uint32_t timestamp = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());

            result.fruitCount = std::min(static_cast<uint8_t>(detections.size()),
                                        MAX_DYNAMIC_FRUIT_COUNT);
            for (uint8_t i = 0; i < result.fruitCount; ++i) {
                result.fruitInfoWithTimestamp[i].timestamp = timestamp;
                result.fruitInfoWithTimestamp[i].fruitInfo = detections[i];
            }

            std::cout << "[CvModel] Dynamic: Tracked " << (int)result.fruitCount << " fruits" << std::endl;
        }
    } else if (!frame.empty()) {
        // 模拟模式
        result.fruitCount = 1;
        result.fruitInfoWithTimestamp[0].timestamp = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        result.fruitInfoWithTimestamp[0].fruitInfo.fruitType = FruitType::APPLE;
        result.fruitInfoWithTimestamp[0].fruitInfo.freshness = FreshnessLevel::FRESH;
        result.fruitInfoWithTimestamp[0].fruitInfo.locationX = 150;
        result.fruitInfoWithTimestamp[0].fruitInfo.locationY = 120;

        std::cout << "[CvModel] Dynamic (SIM): Tracked " << (int)result.fruitCount << " fruits" << std::endl;
    }

    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_dynamicRecognitionResult = result;
    }

    SetDynamicRecognitionStatus(RECOGNITION_IDLE);
}

StaticRecognitionResult CvModelManager::GetStaticResult() {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_staticRecognitionResult;
}

DynamicRecognitionResult CvModelManager::GetDynamicResult() {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_dynamicRecognitionResult;
}