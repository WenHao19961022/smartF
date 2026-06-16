#ifndef CV_MODEL_MANAGER_H
#define CV_MODEL_MANAGER_H

#include "../include/inference_engine.h"
#include "../api/cv_model_api.h"
#include <mutex>
#include <atomic>
#include <memory>
#include <vector>
#include <map>
#include <set>

// ==================== 常量 ====================
const bool kInitFinished = true;
const bool kInitUnfinished = false;
const bool kRecognitionSwitchOn = true;
const bool kRecognitionSwitchOff = false;
const bool kRecognitionIdle = true;
const bool kRecognitionBusy = false;
const int kStaticDetectionCount = 5;
const int kStaticConfirmThreshold = 3;  // 多数确认阈值（≥3/5）

// 动态识别新常量
const int kDynamicWindowSize = 30;       // 滑动窗口帧数
const int kDynamicBaselineFrames = 10;  // 开关ON后前N帧确立基准
const float kDynamicDisappearRatio = 0.3f;  // 出现率<30%判定消失（TAKE_OUT）
const float kDynamicAppearRatio = 0.7f;     // 出现率>70%判定新增（PUT_IN）
const int kDynamicPositionTolerance = 20;   // 位置容差（像素）
const int kDynamicEventCooldownFrames = 15; // 事件确认后的短冷却，避免同一动作重复上报

// ==================== 水果状态追踪项 ====================
// 用于追踪每个被监测水果的出现/消失状态
struct TrackedFruitState {
    FruitType fruitType;
    uint8_t locationX;
    uint8_t locationY;
    bool isBaseline;           // 是否为基准水果（开门时就存在）
    bool alreadyConfirmed;     // 是否已确认变化并记录
    int continuousCount;       // 连续出现帧数（用于新增检测）
    int continuousAbsentCount;// 连续消失帧数（用于消失检测）
    int totalAppearCount;      // 窗口内总出现帧数
    int cooldownFrames;        // 事件确认后的冷却帧数
};

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
    void SetStaticRecognitionSwitch(bool active) { mStaticRecognitionSwitch.store(active); }
    void SetDynamicRecognitionSwitch(bool active) { mDynamicRecognitionSwitch.store(active); }

    // 结果获取
    StaticRecognitionResult GetStaticResult();
    DynamicRecognitionResult GetDynamicResult();

private:
    CvModelManager();
    ~CvModelManager();

    CvModelManager(const CvModelManager&) = delete;
    CvModelManager& operator=(const CvModelManager&) = delete;

    // 静态识别
    void StaticRecognitionInternal();

    // 动态识别（新版）
    void DynamicRecognitionLoop();       // 动态识别主循环
    void DynamicEstablishBaseline();     // 确立基准状态
    void DynamicMonitoringStep();        // 监测步骤（每帧调用）
    void DynamicCheckChanges();          // 检测变化并记录事件
    int CountWindowAppearances(const TrackedFruitState& tracked) const;

    // 工具方法
    bool IsFruitSimilar(const FruitInfo& a, const FruitInfo& b) const;
    bool HasTrackedFruit(FruitType type, uint8_t x, uint8_t y);
    void RecordChangeEvent(FruitType type, FruitChangeAction action, uint8_t x, uint8_t y);
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

    std::unique_ptr<InferenceEngine> mInferenceEngine;
    std::mutex mDataMutex;
    StaticRecognitionResult mStaticRecognitionResult;
    DynamicRecognitionResult mDynamicRecognitionResult;

    // 动态识别新数据
    std::vector<TrackedFruitState> mTrackedFruits;          // 被追踪的水果列表
    std::vector<std::vector<FruitInfo>> mWindowFrames;       // 滑动窗口（最近N帧检测结果）
    int mBaselineFrameCount;                                // 已采集的基准帧数
    bool mBaselineEstablished;                             // 基准是否已确立
    std::mutex mDynamicMutex;                               // 动态识别专用锁

    // 静态识别（保留兼容）
    std::vector<std::vector<FruitInfo>> mSlidingWindowResults;
    std::mutex mSlidingWindowMutex;
};

// ==================== 全局API（兼容旧接口） ====================
bool CvModelInit();
void CvModelMainLoop();
bool IsCvModelReady();
bool IsCameraReady();
void StartStaticRecognition();
void StartDynamicRecognition();
void StopStaticRecognition();
void StopDynamicRecognition();
bool IsStaticRecognitionIdle();
bool IsDynamicRecognitionIdle();
StaticRecognitionResult GetStaticRecognitionResult();
DynamicRecognitionResult GetDynamicRecognitionResult();

#endif // CV_MODEL_MANAGER_H
