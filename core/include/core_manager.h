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