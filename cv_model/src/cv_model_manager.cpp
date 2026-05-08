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
    std::string onnxPath = ConfigManager::GetInstance().GetString("cv.model_onnx_path", "cv_model/yolov8/yolo12n.onnx");

    try {
        mInferenceEngine = std::make_unique<InferenceEngine>(enginePath);
        if (mInferenceEngine->IsCpuMode()) {
            LOG_PRINT("[CvModel]", "Running in CPU mode (ONNX: " << onnxPath << ")");
        } else {
            LOG_PRINT("[CvModel]", "TensorRT Engine Initialized Successfully.");
        }
        SetReady();
        return true;
    } catch (const std::exception& e) {
        LOG_PRINT("[CvModel]", "CV Model init failed: " << e.what());
        return false;
    }
}

void CvModelManager::MainLoop() {
    LOG_PRINT("[CvModel]", "MainLoop started");
    while (true) {
        if (!IsCvModelReady()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        if (IsStaticRecognitionSwitchOn() && IsStaticRecognitionIdle()) {
            LOG_PRINT("[CvModel]", "MainLoop: StaticRecognition triggered (switch=ON, status=IDLE)");
            StaticRecognitionInternal();
        }

        if (IsDynamicRecognitionSwitchOn() && IsDynamicRecognitionIdle()) {
            LOG_PRINT("[CvModel]", "MainLoop: DynamicRecognition triggered (switch=ON, status=IDLE)");
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
    LOG_PRINT("[CvModel]", "=== StaticRecognitionInternal START ===");
    SetStaticRecognitionStatus(kRecognitionBusy);

    std::map<std::tuple<FruitType, uint8_t, uint8_t>, int> fruitCounts;
    StaticRecognitionResult result = {};
    result.timestamp = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    LOG_PRINT("[CvModel]", "Static recognition target: " << kStaticDetectionCount << " detections");
    LOG_PRINT("[CvModel]", "Timestamp: " << result.timestamp);

    int successfulDetections = 0;
    auto startTime = std::chrono::steady_clock::now();

    for (int detectionIndex = 0; detectionIndex < kStaticDetectionCount; ++detectionIndex) {
        if (!IsStaticRecognitionSwitchOn()) {
            LOG_PRINT("[CvModel]", "Static recognition cancelled at detection " << detectionIndex << " (switch turned OFF)");
            break;
        }

        cv::Mat frame = CameraModule::GetInstance().GetLatestFrame();

        if (!frame.empty() && mInferenceEngine) {
            LOG_PRINT("[CvModel]", "Detection " << (detectionIndex + 1) << "/" << kStaticDetectionCount
                      << " | Frame: " << frame.cols << "x" << frame.rows << " channels=" << frame.channels());
            std::vector<float> rawOutput;
            if (mInferenceEngine->Infer(frame, rawOutput)) {
                auto detections = PostProcessYOLO(rawOutput, mInferenceEngine->GetOutputDims());

                for (const auto& det : detections) {
                    auto key = std::make_tuple(det.fruitType, det.locationX, det.locationY);
                    fruitCounts[key]++;
                }

                successfulDetections++;
                LOG_PRINT("[CvModel]", "Detection " << (detectionIndex + 1) << " - Detected " << detections.size() << " fruits");
                // 打印每个检测的详细信息
                for (const auto& det : detections) {
                    LOG_PRINT("[CvModel]", "  - Type: " << (int)det.fruitType
                              << " | Pos: (" << (int)det.locationX << "," << (int)det.locationY << ")"
                              << " | Freshness: " << (int)det.freshness);
                }
            } else {
                LOG_PRINT("[CvModel]", "Detection " << (detectionIndex + 1) << " - Inference failed");
            }
        } else if (!frame.empty()) {
            LOG_PRINT("[CvModel]", "Detection " << (detectionIndex + 1) << " - Inference engine not initialized");
        } else {
            LOG_PRINT("[CvModel]", "Detection " << (detectionIndex + 1) << " - Empty frame from camera, aborting static recognition");
            break;
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    LOG_PRINT("[CvModel]", "Static recognition completed " << successfulDetections << "/" << kStaticDetectionCount
              << " detections in " << durationMs << "ms");

    // Collect fruits that appeared in all detections
    std::vector<FruitInfo> finalDetections;
    int confirmedFruits = 0;
    for (const auto& pair : fruitCounts) {
        LOG_PRINT("[CvModel]", "Fruit candidate: Type=" << (int)std::get<0>(pair.first)
                  << " Pos=(" << (int)std::get<1>(pair.first) << "," << (int)std::get<2>(pair.first) << ")"
                  << " | Count=" << pair.second << "/" << kStaticDetectionCount);
        if (pair.second == kStaticDetectionCount) {
            FruitInfo info;
            std::tie(info.fruitType, info.locationX, info.locationY) = pair.first;
            info.freshness = FreshnessLevel::Fresh; // Default, or could be set based on logic
            finalDetections.push_back(info);
            confirmedFruits++;
        }
    }

    LOG_PRINT("[CvModel]", "Final confirmed fruits: " << confirmedFruits << " (must appear in ALL " << kStaticDetectionCount << " detections)");

    result.fruitCount = std::min(static_cast<uint8_t>(finalDetections.size()), kMaxStaticFruitCount);
    for (uint8_t i = 0; i < result.fruitCount; ++i) {
        result.fruits[i] = finalDetections[i];
    }

    {
        std::lock_guard<std::mutex> lock(mDataMutex);
        mStaticRecognitionResult = result;
    }

    LOG_PRINT("[CvModel]", "Static result stored: fruitCount=" << (int)result.fruitCount
              << " | timestamp=" << result.timestamp);
    LOG_PRINT("[CvModel]", "=== StaticRecognitionInternal END ===");

    SetStaticRecognitionSwitch(kRecognitionSwitchOff);
    SetStaticRecognitionStatus(kRecognitionIdle);
}

void CvModelManager::DynamicRecognitionInternal() {
    LOG_PRINT("[CvModel]", "=== DynamicRecognitionInternal START ===");
    SetDynamicRecognitionStatus(kRecognitionBusy);

    auto startTime = std::chrono::steady_clock::now();
    cv::Mat frame = CameraModule::GetInstance().GetLatestFrame();
    DynamicRecognitionResult result = {};

    if (!frame.empty() && mInferenceEngine) {
        LOG_PRINT("[CvModel]", "Dynamic: Frame " << frame.cols << "x" << frame.rows);
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

            LOG_PRINT("[CvModel]", "Dynamic: Detected " << (int)result.fruitCount << " fruits | timestamp=" << timestamp);
            for (uint8_t i = 0; i < result.fruitCount; ++i) {
                LOG_PRINT("[CvModel]", "  Dynamic[" << (int)i << "]: Type=" << (int)result.fruitInfoWithTimestamp[i].fruitInfo.fruitType
                          << " | Pos=(" << (int)result.fruitInfoWithTimestamp[i].fruitInfo.locationX
                          << "," << (int)result.fruitInfoWithTimestamp[i].fruitInfo.locationY << ")"
                          << " | Freshness=" << (int)result.fruitInfoWithTimestamp[i].fruitInfo.freshness);
            }
        } else {
            LOG_PRINT("[CvModel]", "Dynamic: Inference failed");
        }
    } else if (frame.empty()) {
        LOG_PRINT("[CvModel]", "Dynamic: Empty frame (camera may not be ready)");
    } else {
        LOG_PRINT("[CvModel]", "Dynamic: Inference engine not initialized");
    }

    {
        std::lock_guard<std::mutex> lock(mDataMutex);
        mDynamicRecognitionResult = result;
    }

    auto endTime = std::chrono::steady_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    LOG_PRINT("[CvModel]", "Dynamic result stored: fruitCount=" << (int)result.fruitCount
              << " | duration=" << durationMs << "ms");
    LOG_PRINT("[CvModel]", "=== DynamicRecognitionInternal END ===");

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
