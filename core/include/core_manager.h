#ifndef CORE_MANAGER_H
#define CORE_MANAGER_H

#include <chrono>
#include <vector>
#include <map>
#include <string>
#include "./external_apis.h"
#include "./inventory_manager.h"

// 记录伴随整个开门周期的重量流数据点
struct WeightDataPoint {
    uint32_t timestampMs; 
    uint16_t weight;
};

class CoreManager {
public:
    void Init();
    void Run();

private:
    void HandleDoorOpen();
    void HandleDoorClose();
    void CheckTimers();
    void ProcessStaticResultOnly();
    void TrySendStartupNotification();

    // V5.0: 滑动窗口稳态拼贴算法
    int32_t CalculateLocalDeltaW(uint32_t startTsMs, uint32_t endTsMs);
    // 获取当前毫秒时间戳辅助函数
    uint32_t GetCurrentTimeMs(); 

    bool mRunning = true;
    bool mLastDoorState = false;
    bool mIsStaticWaiting = false;
    bool mStartupNotificationQueued = false;
    uint16_t mBaseWeight = 0;
    uint32_t mDoorOpenTimestamp = 0; 
    uint32_t mDeviceId = 10001;
    std::chrono::seconds mStaticInterval{60};  

    std::chrono::steady_clock::time_point mLastStaticTime;
    std::chrono::steady_clock::time_point mNextStartupNotifyAttempt;

    std::vector<WeightDataPoint> mWeightStream; 
    InventoryManager mInventoryManager;
};

#endif // CORE_MANAGER_H
