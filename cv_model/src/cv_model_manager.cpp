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
#include <algorithm>
#include <numeric>

CvModelManager& CvModelManager::GetInstance() {
    static CvModelManager instance;
    return instance;
}

CvModelManager::CvModelManager()
    : mBaselineFrameCount(0)
    , mBaselineEstablished(false) {
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

    {
        std::lock_guard<std::mutex> lock(mDynamicMutex);
        mTrackedFruits.clear();
        mWindowFrames.clear();
        mBaselineFrameCount = 0;
        mBaselineEstablished = false;
    }

    std::string enginePath = ConfigManager::GetInstance().GetString("cv.model_path", "cv_model/yolov8/yolov8s.engine");
    std::string onnxPath = ConfigManager::GetInstance().GetString("cv.model_onnx_path", "cv_model/yolov8/yolov8s.onnx");

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

// ==================== MainLoop ====================
void CvModelManager::MainLoop() {
    LOG_PRINT("[CvModel]", "MainLoop started");
    while (true) {
        if (!IsCvModelReady()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // --- 静态识别触发 ---
        if (IsStaticRecognitionSwitchOn() && IsStaticRecognitionIdle()) {
            LOG_PRINT("[CvModel]", "MainLoop: StaticRecognition triggered");
            StaticRecognitionInternal();
        }

        // --- 动态识别：开关ON & 动态空闲 → 进入动态识别流程 ---
        if (IsDynamicRecognitionSwitchOn() && IsDynamicRecognitionIdle()) {
            LOG_PRINT("[CvModel]", "MainLoop: DynamicRecognition triggered (switch=ON, status=IDLE)");
            DynamicRecognitionLoop();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// ==================== YOLO后处理 ====================
// ==================== YOLO Post-Processing ====================
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

    // --- ADDED: Define class labels mapping ---
    const std::vector<std::string> class_labels = {
        "fresh_apple", "fresh_banana", "fresh_orange", 
        "rotten_apple", "rotten_banana", "rotten_orange"
    };

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

    // If there are detections, print a separator for this frame
    if (!detections.empty()) {
        std::cout << "============ 检测到目标 ==========" << std::endl;
    }

    for (size_t i = 0; i < detections.size(); ++i) {
        if (suppressed[i]) continue;

        const auto& det = detections[i];
        
        // --- ADDED: Print [ID] Label directly to terminal ---
        std::string label_name = (det.classId < class_labels.size()) ? class_labels[det.classId] : "Unknown";
        std::cout << "[" << det.classId << "] " << label_name << std::endl;

        FruitInfo info;
        info.fruitType = static_cast<FruitType>(det.classId + 1);
        info.locationX = static_cast<uint8_t>(std::min(255, static_cast<int>(det.cx)));
        info.locationY = static_cast<uint8_t>(std::min(255, static_cast<int>(det.cy)));
        info.freshness = FreshnessLevel::Fresh;

        results.push_back(info);

        // NMS logic
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
// ==================== 静态识别 ====================
void CvModelManager::StaticRecognitionInternal() {
    LOG_PRINT("[CvModel]", "=== StaticRecognitionInternal START ===");
    SetStaticRecognitionStatus(kRecognitionBusy);

    std::map<std::tuple<FruitType, uint8_t, uint8_t>, int> fruitCounts;
    StaticRecognitionResult result = {};
    result.timestamp = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    int successfulDetections = 0;
    auto startTime = std::chrono::steady_clock::now();

    for (int detectionIndex = 0; detectionIndex < kStaticDetectionCount; ++detectionIndex) {
        if (!IsStaticRecognitionSwitchOn()) {
            LOG_PRINT("[CvModel]", "Static recognition cancelled at detection " << detectionIndex);
            break;
        }

        cv::Mat frame = CameraModule::GetInstance().GetLatestFrame();

        if (!frame.empty() && mInferenceEngine) {
            std::vector<float> rawOutput;
            if (mInferenceEngine->Infer(frame, rawOutput)) {
                auto detections = PostProcessYOLO(rawOutput, mInferenceEngine->GetOutputDims());
                for (const auto& det : detections) {
                    auto key = std::make_tuple(det.fruitType, det.locationX, det.locationY);
                    LOG_PRINT("[CvModel]", "  Detection " << detectionIndex << ": type=" << (int)det.fruitType
                              << " pos=(" << (int)det.locationX << "," << (int)det.locationY << ")");
                    fruitCounts[key]++;
                }
                successfulDetections++;
            }
        } else if (frame.empty()) {
            continue;
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    LOG_PRINT("[CvModel]", "Static recognition completed " << successfulDetections << "/" << kStaticDetectionCount
              << " detections in " << durationMs << "ms");

    // 多数确认
    std::vector<FruitInfo> finalDetections;
    for (const auto& pair : fruitCounts) {
        if (pair.second >= kStaticConfirmThreshold) {
            FruitInfo info;
            std::tie(info.fruitType, info.locationX, info.locationY) = pair.first;
            info.freshness = FreshnessLevel::Fresh;
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

    LOG_PRINT("[CvModel]", "=== StaticRecognitionInternal END === fruitCount=" << (int)result.fruitCount);

    SetStaticRecognitionSwitch(kRecognitionSwitchOff);
    SetStaticRecognitionStatus(kRecognitionIdle);
}

// ==================== 动态识别主循环（新版） ====================
void CvModelManager::DynamicRecognitionLoop() {
    LOG_PRINT("[CvModel]", "=== DynamicRecognitionLoop START ===");
    SetDynamicRecognitionStatus(kRecognitionBusy);

    // 初始化动态识别状态
    {
        std::lock_guard<std::mutex> lock(mDynamicMutex);
        mTrackedFruits.clear();
        mWindowFrames.clear();
        mBaselineFrameCount = 0;
        mBaselineEstablished = false;
    }

    LOG_PRINT("[CvModel]", "DynamicRecognitionLoop: entering baseline + monitoring loop");

    // 主循环：持续运行直到开关关闭
    while (IsDynamicRecognitionSwitchOn()) {
        // 获取一帧并推理
        cv::Mat frame = CameraModule::GetInstance().GetLatestFrame();
        if (frame.empty() || !mInferenceEngine) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        std::vector<float> rawOutput;
        if (!mInferenceEngine->Infer(frame, rawOutput)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        auto detections = PostProcessYOLO(rawOutput, mInferenceEngine->GetOutputDims());

        {
            std::lock_guard<std::mutex> lock(mDynamicMutex);

            if (!mBaselineEstablished) {
                // ===== 基准建立阶段 =====
                mWindowFrames.push_back(detections);
                mBaselineFrameCount++;

                if (mBaselineFrameCount >= kDynamicBaselineFrames) {
                    DynamicEstablishBaseline();
                    mBaselineEstablished = true;
                    LOG_PRINT("[CvModel]", "Baseline established, switching to monitoring mode");
                }
            } else {
                // ===== 监测阶段 =====
                // 加入滑动窗口
                mWindowFrames.push_back(detections);
                if (static_cast<int>(mWindowFrames.size()) > kDynamicWindowSize) {
                    mWindowFrames.erase(mWindowFrames.begin());
                }

                // 更新追踪水果状态
                DynamicMonitoringStep();
                // 检测变化
                DynamicCheckChanges();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 开关被关闭，构建最终结果
    LOG_PRINT("[CvModel]", "DynamicRecognitionLoop: switch OFF, building final result");

    DynamicRecognitionResult result = {};
    {
        std::lock_guard<std::mutex> dynLock(mDynamicMutex);

        // 如果基准还没建立就结束了，结果为空
        if (mBaselineEstablished) {
            // 做最后一轮变化检测（处理还在pending中的变化）
            DynamicCheckChanges();
        }

        // 从 trackedFruits 中收集所有 confirmed 变化事件
        // 同时也从动态结果中读取之前记录的事件
        {
            std::lock_guard<std::mutex> dataLock(mDataMutex);
            result = mDynamicRecognitionResult;
        }
    }

    LOG_PRINT("[CvModel]", "DynamicRecognitionLoop: eventCount=" << (int)result.eventCount);
    for (uint8_t i = 0; i < result.eventCount; ++i) {
        LOG_PRINT("[CvModel]", "  Event[" << (int)i << "]: type=" << (int)result.events[i].fruitType
                  << " action=" << (result.events[i].action == FruitChangeAction::PUT_IN ? "PUT_IN" : "TAKE_OUT")
                  << " ts=" << result.events[i].timestamp);
    }
    LOG_PRINT("[CvModel]", "=== DynamicRecognitionLoop END ===");

    // 清理动态识别状态
    {
        std::lock_guard<std::mutex> lock(mDynamicMutex);
        mTrackedFruits.clear();
        mWindowFrames.clear();
        mBaselineFrameCount = 0;
        mBaselineEstablished = false;
    }

    SetDynamicRecognitionStatus(kRecognitionIdle);
}

// ==================== 确立基准 ====================
void CvModelManager::DynamicEstablishBaseline() {
    // 统计每种水果在基准帧中的出现次数
    // 使用位置容差合并同类水果
    struct Candidate {
        FruitType type;
        float sumX, sumY;
        int count;
    };
    std::vector<Candidate> candidates;

    for (const auto& frameDetections : mWindowFrames) {
        std::vector<bool> candidateUsed(candidates.size(), false);

        for (const auto& fruit : frameDetections) {
            bool matched = false;
            for (size_t j = 0; j < candidates.size(); ++j) {
                if (candidates[j].type != fruit.fruitType) continue;

                float dx = fruit.locationX - (candidates[j].sumX / candidates[j].count);
                float dy = fruit.locationY - (candidates[j].sumY / candidates[j].count);
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist < kDynamicPositionTolerance) {
                    candidates[j].sumX += fruit.locationX;
                    candidates[j].sumY += fruit.locationY;
                    candidates[j].count++;
                    matched = true;
                    break;
                }
            }

            if (!matched) {
                candidates.push_back({fruit.fruitType,
                                      static_cast<float>(fruit.locationX),
                                      static_cast<float>(fruit.locationY), 1});
            }
        }
    }

    // 在 >= 50% 的基准帧中出现的水果视为基准水果
    int threshold = static_cast<int>(std::ceil(kDynamicBaselineFrames * 0.5f));
    LOG_PRINT("[CvModel]", "EstablishBaseline: " << candidates.size() << " candidates, threshold=" << threshold);

    for (const auto& cand : candidates) {
        if (cand.count >= threshold) {
            TrackedFruitState state;
            state.fruitType = cand.type;
            state.locationX = static_cast<uint8_t>(std::min(255, static_cast<int>(cand.sumX / cand.count)));
            state.locationY = static_cast<uint8_t>(std::min(255, static_cast<int>(cand.sumY / cand.count)));
            state.isBaseline = true;
            state.alreadyConfirmed = false;
            state.continuousCount = 0;
            state.continuousAbsentCount = 0;
            state.totalAppearCount = kDynamicWindowSize;  // 初始认为全窗口都存在

            mTrackedFruits.push_back(state);
            LOG_PRINT("[CvModel]", "  Baseline fruit: type=" << (int)state.fruitType
                      << " pos=(" << (int)state.locationX << "," << (int)state.locationY << ")"
                      << " appeared=" << cand.count << "/" << kDynamicBaselineFrames);
        }
    }

    LOG_PRINT("[CvModel]", "Baseline established: " << mTrackedFruits.size() << " fruits");
}

// ==================== 监测步骤（更新追踪状态） ====================
void CvModelManager::DynamicMonitoringStep() {
    if (mWindowFrames.empty()) return;

    // 取最新一帧的检测结果
    const auto& latestDetections = mWindowFrames.back();

    // 标记每个追踪水果在本帧是否被检测到
    for (auto& tracked : mTrackedFruits) {
        bool found = false;
        for (const auto& det : latestDetections) {
            if (det.fruitType == tracked.fruitType && IsFruitSimilar(det, {tracked.fruitType, tracked.locationX, tracked.locationY, FreshnessLevel::Fresh})) {
                found = true;
                // 更新位置（平滑）
                tracked.locationX = static_cast<uint8_t>((tracked.locationX * 7 + det.locationX * 3) / 10);
                tracked.locationY = static_cast<uint8_t>((tracked.locationY * 7 + det.locationY * 3) / 10);
                break;
            }
        }

        if (found) {
            tracked.continuousCount++;
            tracked.continuousAbsentCount = 0;
            tracked.totalAppearCount++;
        } else {
            tracked.continuousAbsentCount++;
            tracked.continuousCount = 0;
        }
    }

    // 检测是否有新水果出现（不在追踪列表中的）
    for (const auto& det : latestDetections) {
        bool existing = HasTrackedFruit(det.fruitType, det.locationX, det.locationY);
        if (!existing) {
            // 新水果，加入追踪列表
            TrackedFruitState state;
            state.fruitType = det.fruitType;
            state.locationX = det.locationX;
            state.locationY = det.locationY;
            state.isBaseline = false;
            state.alreadyConfirmed = false;
            state.continuousCount = 1;
            state.continuousAbsentCount = 0;
            state.totalAppearCount = 1;
            mTrackedFruits.push_back(state);
            LOG_PRINT("[CvModel]", "New fruit detected: type=" << (int)det.fruitType
                      << " pos=(" << (int)det.locationX << "," << (int)det.locationY << ")");
        }
    }
}

// ==================== 检测变化 ====================
void CvModelManager::DynamicCheckChanges() {
    int windowSize = static_cast<int>(mWindowFrames.size());
    if (windowSize < kDynamicWindowSize) return;  // 窗口未满，不检测

    uint32_t now = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    for (auto& tracked : mTrackedFruits) {
        if (tracked.alreadyConfirmed) continue;

        float ratio = static_cast<float>(tracked.totalAppearCount) / windowSize;

        // 基准水果消失 → TAKE_OUT
        if (tracked.isBaseline && ratio < kDynamicDisappearRatio) {
            RecordChangeEvent(tracked.fruitType, FruitChangeAction::TAKE_OUT,
                              tracked.locationX, tracked.locationY);
            LOG_PRINT("[CvModel]", "CHANGE: TAKE_OUT type=" << (int)tracked.fruitType
                      << " ratio=" << ratio << " (<" << kDynamicDisappearRatio << ")");

            // 确认后翻转状态：该水果现在不在冰箱中，开始监测是否会被重新放入
            tracked.isBaseline = false;
            tracked.alreadyConfirmed = false;
            tracked.continuousCount = 0;
            tracked.continuousAbsentCount = 0;
            tracked.totalAppearCount = 0;
        }

        // 非基准水果出现 → PUT_IN
        if (!tracked.isBaseline && ratio >= kDynamicAppearRatio) {
            RecordChangeEvent(tracked.fruitType, FruitChangeAction::PUT_IN,
                              tracked.locationX, tracked.locationY);
            LOG_PRINT("[CvModel]", "CHANGE: PUT_IN type=" << (int)tracked.fruitType
                      << " ratio=" << ratio << " (>=" << kDynamicAppearRatio << ")");

            // 确认后翻转状态：该水果现在在冰箱中，开始监测是否会被取走
            tracked.isBaseline = true;
            tracked.alreadyConfirmed = false;
            tracked.continuousCount = 0;
            tracked.continuousAbsentCount = 0;
            tracked.totalAppearCount = 0;
        }
    }

    // 清理：完全消失的非基准水果（误检测）移除追踪列表
    mTrackedFruits.erase(
        std::remove_if(mTrackedFruits.begin(), mTrackedFruits.end(),
            [](const TrackedFruitState& tf) {
                return !tf.isBaseline && tf.continuousAbsentCount > kDynamicWindowSize * 2;
            }),
        mTrackedFruits.end());
}

// ==================== 记录变化事件 ====================
void CvModelManager::RecordChangeEvent(FruitType type, FruitChangeAction action, uint8_t x, uint8_t y) {
    uint32_t now = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    std::lock_guard<std::mutex> dataLock(mDataMutex);

    if (mDynamicRecognitionResult.eventCount >= kMaxDynamicEventCount) {
        LOG_PRINT("[CvModel]", "WARNING: event list full, cannot record more events");
        return;
    }

    uint8_t idx = mDynamicRecognitionResult.eventCount;
    mDynamicRecognitionResult.events[idx].fruitType = type;
    mDynamicRecognitionResult.events[idx].action = action;
    mDynamicRecognitionResult.events[idx].timestamp = now;
    mDynamicRecognitionResult.events[idx].locationX = x;
    mDynamicRecognitionResult.events[idx].locationY = y;
    mDynamicRecognitionResult.eventCount++;

    LOG_PRINT("[CvModel]", "Event recorded [" << (int)idx << "]: type=" << (int)type
              << " action=" << (action == FruitChangeAction::PUT_IN ? "PUT_IN" : "TAKE_OUT")
              << " ts=" << now << " pos=(" << (int)x << "," << (int)y << ")");
}

// ==================== 工具方法 ====================
bool CvModelManager::IsFruitSimilar(const FruitInfo& a, const FruitInfo& b) const {
    if (a.fruitType != b.fruitType) return false;
    int dx = static_cast<int>(a.locationX) - static_cast<int>(b.locationX);
    int dy = static_cast<int>(a.locationY) - static_cast<int>(b.locationY);
    int dist = static_cast<int>(std::sqrt(static_cast<float>(dx * dx + dy * dy)));
    return dist < kDynamicPositionTolerance;
}

bool CvModelManager::HasTrackedFruit(FruitType type, uint8_t x, uint8_t y) {
    FruitInfo target{type, x, y, FreshnessLevel::Fresh};
    for (auto& tracked : mTrackedFruits) {
        FruitInfo trackedInfo{tracked.fruitType, tracked.locationX, tracked.locationY, FreshnessLevel::Fresh};
        if (IsFruitSimilar(target, trackedInfo)) {
            return true;
        }
    }
    return false;
}

// ==================== 结果获取 ====================
StaticRecognitionResult CvModelManager::GetStaticResult() {
    std::lock_guard<std::mutex> lock(mDataMutex);
    return mStaticRecognitionResult;
}

DynamicRecognitionResult CvModelManager::GetDynamicResult() {
    std::lock_guard<std::mutex> lock(mDataMutex);
    return mDynamicRecognitionResult;
}
