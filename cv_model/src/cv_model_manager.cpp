#include "../include/cv_model_manager.h"
#include "../include/camera_model.h"
#include "../../common/include/config_manager.h"
#include "../../common/include/logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <map>
#include <tuple>

CvModelManager& CvModelManager::GetInstance() {
    static CvModelManager instance;
    return instance;
}

CvModelManager::CvModelManager() {
}

CvModelManager::~CvModelManager() {
}

bool CvModelManager::CvModelInit() {
    mInitStatus.store(kInitUnfinished);

    mStaticRecognitionSwitch.store(kRecognitionSwitchOff);
    mDynamicRecognitionSwitch.store(kRecognitionSwitchOff);
    mStaticRecognitionStatus.store(kRecognitionIdle);
    mDynamicRecognitionStatus.store(kRecognitionIdle);

    mDataMutex.lock();
    mStaticRecognitionResult = {};
    mDynamicRecognitionResult = {};
    mDataMutex.unlock();

    std::string enginePath = ConfigManager::GetInstance().GetString("cv.model_path", "cv_model/yolov8/yolo12n.engine");

    try {
        mInferenceEngine = std::make_unique<InferenceEngine>(enginePath);
        SetReady();
        LOG_PRINT("[CvModel]", "TensorRT Engine Initialized Successfully.");
        return true;
    } catch (const std::exception& e) {
        LOG_PRINT("[CvModel]", "TensorRT init failed: " << e.what());
        return false;
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
    int numClasses = outputDims[1] - 4;

    const int kNumFruitClasses = 6;

    struct Detection {
        float cx, cy, w, h;
        float confidence;
        int classId;
    };
    std::vector<Detection> detections;

    for (int i = 0; i < numProposals; ++i) {
        float cx = output[i];
        float cy = output[numProposals + i];
        float bw = output[2 * numProposals + i];
        float bh = output[3 * numProposals + i];

        float maxConf = 0.0f;
        int maxClassId = 0;

        for (int c = 0; c < std::min(numClasses, kNumFruitClasses); ++c) {
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

    std::vector<bool> suppressed(detections.size(), false);

    for (size_t i = 0; i < detections.size(); ++i) {
        if (suppressed[i]) continue;

        const auto& det = detections[i];
        FruitInfo info;
        info.fruitType = static_cast<FruitType>(det.classId + 1);
        info.locationX = static_cast<uint8_t>(std::min(255, static_cast<int>(det.cx)));
        info.locationY = static_cast<uint8_t>(std::min(255, static_cast<int>(det.cy)));
        info.freshness = FreshnessLevel::Fresh;

        results.push_back(info);

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
    SetStaticRecognitionStatus(kRecognitionBusy);

    std::map<std::tuple<FruitType, uint8_t, uint8_t>, int> fruitCounts;
    StaticRecognitionResult result = {};
    result.timestamp = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    for (int detectionIndex = 0; detectionIndex < kStaticDetectionCount; ++detectionIndex) {
        if (!IsStaticRecognitionSwitchOn()) {
            break;
        }

        cv::Mat frame = CameraModule::GetInstance().GetLatestFrame();

        if (!frame.empty() && mInferenceEngine) {
            std::vector<float> rawOutput;
            if (mInferenceEngine->Infer(frame, rawOutput)) {
                auto detections = PostProcessYOLO(rawOutput, mInferenceEngine->GetOutputDims());

                for (const auto& det : detections) {
                    auto key = std::make_tuple(det.fruitType, det.locationX, det.locationY);
                    fruitCounts[key]++;
                }

                LOG_PRINT("[CvModel]", "Static: Detection " << (detectionIndex + 1) << " - Detected " << detections.size() << " fruits");
                // 打印每个检测的详细信息
                for (const auto& det : detections) {
                    LOG_PRINT("[CvModel]", "  - Type: " << (int)det.fruitType);
                }
            }
        } else if (!frame.empty()) {
            LOG_PRINT("[CvModel]", "CameraModule inference engine is not initialized");
        } else {
            break;
        }
    }

    // Collect fruits that appeared in all detections
    std::vector<FruitInfo> finalDetections;
    for (const auto& pair : fruitCounts) {
        if (pair.second == kStaticDetectionCount) {
            FruitInfo info;
            std::tie(info.fruitType, info.locationX, info.locationY) = pair.first;
            info.freshness = FreshnessLevel::Fresh; // Default, or could be set based on logic
            finalDetections.push_back(info);
        }
    }

    result.fruitCount = std::min(static_cast<uint8_t>(finalDetections.size()), kMaxStaticFruitCount);
    for (uint8_t i = 0; i < result.fruitCount; ++i) {
        result.fruits[i] = finalDetections[i];
    }

    {
        std::lock_guard<std::mutex> lock(mDataMutex);
        mStaticRecognitionResult = result;
    }

    SetStaticRecognitionSwitch(kRecognitionSwitchOff);
    SetStaticRecognitionStatus(kRecognitionIdle);
}

void CvModelManager::DynamicRecognitionInternal() {
    SetDynamicRecognitionStatus(kRecognitionBusy);

    cv::Mat frame = CameraModule::GetInstance().GetLatestFrame();
    DynamicRecognitionResult result = {};

    if (!frame.empty() && mInferenceEngine) {
        std::vector<float> rawOutput;
        if (mInferenceEngine->Infer(frame, rawOutput)) {
            auto detections = PostProcessYOLO(rawOutput, mInferenceEngine->GetOutputDims());

            uint32_t timestamp = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());

            result.fruitCount = std::min(static_cast<uint8_t>(detections.size()), kMaxDynamicFruitCount);
            for (uint8_t i = 0; i < result.fruitCount; ++i) {
                result.fruitInfoWithTimestamp[i].timestamp = timestamp;
                result.fruitInfoWithTimestamp[i].fruitInfo = detections[i];
            }

            LOG_PRINT("[CvModel]", "Dynamic: Tracked " << (int)result.fruitCount << " fruits");
        }
    }

    {
        std::lock_guard<std::mutex> lock(mDataMutex);
        mDynamicRecognitionResult = result;
    }

    SetDynamicRecognitionSwitch(kRecognitionSwitchOff);
    SetDynamicRecognitionStatus(kRecognitionIdle);
}

StaticRecognitionResult CvModelManager::GetStaticResult() {
    std::lock_guard<std::mutex> lock(mDataMutex);
    return mStaticRecognitionResult;
}

DynamicRecognitionResult CvModelManager::GetDynamicResult() {
    std::lock_guard<std::mutex> lock(mDataMutex);
    return mDynamicRecognitionResult;
}
