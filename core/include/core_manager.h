#ifndef CORE_MANAGER_H
#define CORE_MANAGER_H

#include <chrono>
#include "./external_apis.h"
#include "./inventory_manager.h"

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

    bool mRunning = true;
    bool mLastDoorState = false;
    bool mIsStaticWaiting = false;
    uint16_t mBaseWeight = 0;
    uint32_t mDoorOpenTimestamp = 0;
    uint32_t mMessageIdCounter = 0;
    uint32_t mDeviceId = 10001;
    std::chrono::seconds mStaticInterval{60};  // 静态盘点间隔：1分钟（联调测试用）

    std::chrono::steady_clock::time_point mLastStaticTime;

    InventoryManager mInventoryManager;
};

#endif // CORE_MANAGER_H