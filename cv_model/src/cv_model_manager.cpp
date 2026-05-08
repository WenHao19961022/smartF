#include "../include/cv_model_manager.h"
#include "../include/camera_model.h"
#include "../../common/include/config_manager.h"
#include "../../common/include/logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <map>
#include <set>
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
    mDynamicRecognitionRunning.store(false);

    mDataMutex.lock();
    mStaticRecognitionResult = {};
    mDynamicRecognitionResult = {};
    mDataMutex.unlock();

    mSlidingWindowMutex.lock();
    mSlidingWindowResults.clear();
    mSlidingWindowMutex.unlock();

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

bool CvModelManager::IsCameraReady() const {
    return CameraModule::GetInstance().IsOpened();
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

        // 动态采样模式：开关打开且运行中，持续采样帧
        if (IsDynamicRecognitionSwitchOn() && IsDynamicRecognitionIdle()) {
            // StartDynamicRecognition 调用：初始化滑动窗口 + 进入采样模式
            StartDynamicSampling();
        }

        // 动态采样中：持续采样
        if (mDynamicRecognitionRunning.load()) {
            DynamicSamplingStep();
        }

        // 动态识别结束：开关关闭，触发综合
        if (!IsDynamicRecognitionSwitchOn() && IsDynamicRecognitionIdle() && mDynamicRecognitionRunning.load()) {
            // StopDynamicRecognition 调用：停止采样 + 综合结果
            StopDynamicSampling();
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
            // 空帧：跳过本次，继续下一帧（问题4修复）
            LOG_PRINT("[CvModel]", "Detection " << (detectionIndex + 1) << " - Empty frame, skipping...");
            continue;
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    LOG_PRINT("[CvModel]", "Static recognition completed " << successfulDetections << "/" << kStaticDetectionCount
              << " detections in " << durationMs << "ms");

    // 多数确认：水果出现次数 ≥ kStaticConfirmThreshold（≥3/5）（问题3修复）
    std::vector<FruitInfo> finalDetections;
    int confirmedFruits = 0;
    for (const auto& pair : fruitCounts) {
        LOG_PRINT("[CvModel]", "Fruit candidate: Type=" << (int)std::get<0>(pair.first)
                  << " Pos=(" << (int)std::get<1>(pair.first) << "," << (int)std::get<2>(pair.first) << ")"
                  << " | Count=" << pair.second << "/" << kStaticDetectionCount);
        if (pair.second >= kStaticConfirmThreshold) {
            FruitInfo info;
            std::tie(info.fruitType, info.locationX, info.locationY) = pair.first;
            info.freshness = FreshnessLevel::Fresh;
            finalDetections.push_back(info);
            confirmedFruits++;
        }
    }

    LOG_PRINT("[CvModel]", "Final confirmed fruits: " << confirmedFruits << " (must appear in ≥" << kStaticConfirmThreshold << "/" << kStaticDetectionCount << " detections)");

    // 兜底：如果有效检测次数 < threshold，打印警告但不终止
    if (successfulDetections < kStaticConfirmThreshold) {
        LOG_PRINT("[CvModel]", "WARNING: Only " << successfulDetections << " successful detections (< threshold " << kStaticConfirmThreshold << ")");
    }

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

void CvModelManager::StartDynamicSampling() {
    LOG_PRINT("[CvModel]", "=== StartDynamicSampling: Initializing sliding window ===");
    mDynamicRecognitionRunning.store(true);
    {
        std::lock_guard<std::mutex> lock(mSlidingWindowMutex);
        mSlidingWindowResults.clear();
    }
    LOG_PRINT("[CvModel]", "Sliding window initialized, sampling will begin");
}

void CvModelManager::StopDynamicSampling() {
    LOG_PRINT("[CvModel]", "=== StopDynamicSampling: Aggregating results ===");
    mDynamicRecognitionRunning.store(false);
    DynamicAggregateResults();
    SetDynamicRecognitionStatus(kRecognitionIdle);
}

void CvModelManager::DynamicSamplingStep() {
    cv::Mat frame = CameraModule::GetInstance().GetLatestFrame();
    if (frame.empty() || !mInferenceEngine) {
        return;
    }

    std::vector<float> rawOutput;
    if (!mInferenceEngine->Infer(frame, rawOutput)) {
        return;
    }

    auto detections = PostProcessYOLO(rawOutput, mInferenceEngine->GetOutputDims());

    {
        std::lock_guard<std::mutex> lock(mSlidingWindowMutex);
        mSlidingWindowResults.push_back(detections);
        // 限制滑动窗口大小
        if (mSlidingWindowResults.size() > kDynamicSlidingWindowSize) {
            mSlidingWindowResults.erase(mSlidingWindowResults.begin());
        }
        LOG_PRINT("[CvModel]", "Dynamic sampling: frame captured, window size=" << mSlidingWindowResults.size());
    }
}

void CvModelManager::DynamicAggregateResults() {
    LOG_PRINT("[CvModel]", "=== DynamicAggregateResults START ===");
    SetDynamicRecognitionStatus(kRecognitionBusy);

    std::vector<std::vector<FruitInfo>> window;
    {
        std::lock_guard<std::mutex> lock(mSlidingWindowMutex);
        window = mSlidingWindowResults;
    }

    // 滑动窗口为空时返回空结果
    if (window.empty()) {
        LOG_PRINT("[CvModel]", "Dynamic: Sliding window empty, returning empty result");
        DynamicRecognitionResult emptyResult = {};
        {
            std::lock_guard<std::mutex> lock(mDataMutex);
            mDynamicRecognitionResult = emptyResult;
        }
        return;
    }

    int windowSize = static_cast<int>(window.size());
    int confirmThreshold = static_cast<int>(std::ceil(windowSize * kDynamicConfirmRatio));
    LOG_PRINT("[CvModel]", "Dynamic: windowSize=" << windowSize << " confirmThreshold=" << confirmThreshold
              << " (>=40% of frames)");

    // Step 1: 统计每种水果的出现次数（按 type+location 分组）
    std::map<std::tuple<FruitType, uint8_t, uint8_t>, int> fruitCounts;
    for (const auto& frameResults : window) {
        std::set<std::tuple<FruitType, uint8_t, uint8_t>> seenInFrame;
        for (const auto& fruit : frameResults) {
            auto key = std::make_tuple(fruit.fruitType, fruit.locationX, fruit.locationY);
            // 每帧同类水果只计一次（避免单帧重复检测导致的权重偏差）
            if (seenInFrame.find(key) == seenInFrame.end()) {
                fruitCounts[key]++;
                seenInFrame.insert(key);
            }
        }
    }

    // Step 2: 出现次数 > threshold 的水果被确认
    std::vector<FruitInfo> confirmedFruits;
    for (const auto& pair : fruitCounts) {
        LOG_PRINT("[CvModel]", "Dynamic candidate: Type=" << (int)std::get<0>(pair.first)
                  << " Pos=(" << (int)std::get<1>(pair.first) << "," << (int)std::get<2>(pair.first) << ")"
                  << " | Appeared in " << pair.second << "/" << windowSize << " frames");
        if (pair.second >= confirmThreshold) {
            FruitInfo info;
            std::tie(info.fruitType, info.locationX, info.locationY) = pair.first;
            info.freshness = FreshnessLevel::Fresh;
            confirmedFruits.push_back(info);
        }
    }

    // Step 3: 构建结果（包含所有确认的水果类型，问题2修复）
    DynamicRecognitionResult result = {};
    uint32_t timestamp = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    result.fruitCount = std::min(static_cast<uint8_t>(confirmedFruits.size()), kMaxDynamicFruitCount);
    for (uint8_t i = 0; i < result.fruitCount; ++i) {
        result.fruitInfoWithTimestamp[i].timestamp = timestamp;
        result.fruitInfoWithTimestamp[i].fruitInfo = confirmedFruits[i];
    }

    LOG_PRINT("[CvModel]", "Dynamic: Confirmed " << (int)result.fruitCount << " unique fruit types");
    for (uint8_t i = 0; i < result.fruitCount; ++i) {
        LOG_PRINT("[CvModel]", "  Dynamic[" << (int)i << "]: Type=" << (int)result.fruitInfoWithTimestamp[i].fruitInfo.fruitType
                  << " | Pos=(" << (int)result.fruitInfoWithTimestamp[i].fruitInfo.locationX
                  << "," << (int)result.fruitInfoWithTimestamp[i].fruitInfo.locationY << ")");
    }

    {
        std::lock_guard<std::mutex> lock(mDataMutex);
        mDynamicRecognitionResult = result;
    }

    // 清理滑动窗口
    {
        std::lock_guard<std::mutex> lock(mSlidingWindowMutex);
        mSlidingWindowResults.clear();
    }

    LOG_PRINT("[CvModel]", "=== DynamicAggregateResults END ===");
}

// 保留原 DynamicRecognitionInternal 以兼容旧调用路径（内部直接调用综合逻辑）
void CvModelManager::DynamicRecognitionInternal() {
    // 当 StartDynamicSampling 已采集过数据时，直接综合
    if (mDynamicRecognitionRunning.load() || !mSlidingWindowResults.empty()) {
        StopDynamicSampling();
    } else {
        // 兜底：旧调用路径，单帧推理（退化模式）
        LOG_PRINT("[CvModel]", "=== DynamicRecognitionInternal (legacy single-frame mode) ===");
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
            }
        }

        {
            std::lock_guard<std::mutex> lock(mDataMutex);
            mDynamicRecognitionResult = result;
        }

        SetDynamicRecognitionSwitch(kRecognitionSwitchOff);
        SetDynamicRecognitionStatus(kRecognitionIdle);
    }
}

StaticRecognitionResult CvModelManager::GetStaticResult() {
    std::lock_guard<std::mutex> lock(mDataMutex);
    return mStaticRecognitionResult;
}

DynamicRecognitionResult CvModelManager::GetDynamicResult() {
    std::lock_guard<std::mutex> lock(mDataMutex);
    return mDynamicRecognitionResult;
}
