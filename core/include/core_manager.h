#ifndef CORE_MANAGER_H
#define CORE_MANAGER_H

#include <chrono>
#include "./external_apis.h"
#include "./inventory_manager.h"

// ==================== 常量 ====================
const uint32_t kFridgeDeviceId = 10001;
const std::chrono::seconds kStaticInterval{2 * 3600};

// ==================== 核心管理类 ====================
class CoreManager {
public:
    void Init();
    void Run();

private:
    void HandleDoorOpen();
    void HandleDoorClose();
    void CheckTimers();
    void ProcessStaticResultOnly();

    // --- 自动化调试相关 ---
    void ExecuteCvDebugCycle();      // 调试逻辑主控
    void ResetDebugCycle();          // 重置调试状态

    enum class DebugState { IDLE, DYNAMIC_START, WAITING_DYNAMIC, STATIC_START };
    DebugState mDebugState = DebugState::IDLE;

    std::chrono::steady_clock::time_point mLastDebugTriggerTime; // 上次触发调试的时间
    std::chrono::steady_clock::time_point mDynamicStartTime;    // 动态开启的时间点

    const std::chrono::minutes kDebugInterval{2};              // 2分钟一个周期
    const std::chrono::seconds kDynamicDuration{20};           // 动态等待20秒

    bool mRunning = true;
    bool mLastDoorState = false;
    bool mIsStaticWaiting = false;
    uint16_t mBaseWeight = 0;
    uint32_t mDoorOpenTimestamp = 0;
    uint32_t mMessageIdCounter = 0;

    std::chrono::steady_clock::time_point mLastStaticTime;

    InventoryManager mInventoryManager;
};

#endif // CORE_MANAGER_H