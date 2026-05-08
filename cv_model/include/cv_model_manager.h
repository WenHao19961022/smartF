#ifndef CV_MODEL_MANAGER_H
#define CV_MODEL_MANAGER_H

#include "../include/inference_engine.h"
#include "../api/cv_model_api.h"
#include <mutex>
#include <atomic>
#include <memory>
#include <vector>

// ==================== 常量 ====================
const bool kInitFinished = true;
const bool kInitUnfinished = false;
const bool kRecognitionSwitchOn = true;
const bool kRecognitionSwitchOff = false;
const bool kRecognitionIdle = true;
const bool kRecognitionBusy = false;
const int kStaticDetectionCount = 5;
const int kStaticConfirmThreshold = 3;  // 多数确认阈值（≥3/5）
const int kDynamicSlidingWindowSize = 10;  // 动态识别滑动窗口帧数
const float kDynamicConfirmRatio = 0.4f;   // 动态识别确认比例（40%以上帧出现即确认）

// ==================== CV模型管理类 ====================
class CvModelManager {
public:
    static CvModelManager& GetInstance();

    bool CvModelInit();
    void MainLoop();

    // 状态查询
    bool IsCvModelReady() const { return mInitStatus.load(); }
    bool IsCameraReady() const;
    bool IsStaticRecognitionIdle() const { return mStaticRecognitionStatus.load(); }
    bool IsDynamicRecognitionIdle() const { return mDynamicRecognitionStatus.load(); }
    bool IsDynamicRecognitionRunning() const { return mDynamicRecognitionRunning.load(); }
    void SetStaticRecognitionSwitch(bool active) { mStaticRecognitionSwitch.store(active); }
    void SetDynamicRecognitionSwitch(bool active) { mDynamicRecognitionSwitch.store(active); }

    // 动态识别采样控制
    void StartDynamicSampling();
    void StopDynamicSampling();

    // 结果获取
    StaticRecognitionResult GetStaticResult();
    DynamicRecognitionResult GetDynamicResult();

private:
    CvModelManager();
    ~CvModelManager();

    CvModelManager(const CvModelManager&) = delete;
    CvModelManager& operator=(const CvModelManager&) = delete;

    void StaticRecognitionInternal();
    void DynamicRecognitionInternal();
    void DynamicSamplingStep();           // 单帧采样
    void DynamicAggregateResults();       // 滑动窗口综合

    bool IsStaticRecognitionSwitchOn() const { return mStaticRecognitionSwitch.load(); }
    bool IsDynamicRecognitionSwitchOn() const { return mDynamicRecognitionSwitch.load(); }
    void SetStaticRecognitionStatus(bool status) { mStaticRecognitionStatus.store(status); }
    void SetDynamicRecognitionStatus(bool status) { mDynamicRecognitionStatus.store(status); }
    void SetReady() { mInitStatus.store(kInitFinished); }

    // 成员变量
    std::atomic<bool> mInitStatus{kInitUnfinished};
    std::atomic<bool> mStaticRecognitionSwitch{kRecognitionSwitchOff};
    std::atomic<bool> mDynamicRecognitionSwitch{kRecognitionSwitchOff};
    std::atomic<bool> mStaticRecognitionStatus{kRecognitionIdle};
    std::atomic<bool> mDynamicRecognitionStatus{kRecognitionIdle};
    std::atomic<bool> mDynamicRecognitionRunning{false};

    std::unique_ptr<InferenceEngine> mInferenceEngine;
    std::mutex mDataMutex;
    StaticRecognitionResult mStaticRecognitionResult;
    DynamicRecognitionResult mDynamicRecognitionResult;

    // 动态识别滑动窗口
    std::vector<std::vector<FruitInfo>> mSlidingWindowResults;
    std::mutex mSlidingWindowMutex;
};

#endif // CV_MODEL_MANAGER_H