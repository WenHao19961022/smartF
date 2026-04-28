#pragma once
#include <chrono>
#include "./external_apis.h"

class CoreManager {
private:
    bool running_ = true;
    bool last_door_state_ = false;
    bool is_static_waiting_ = false;
    uint16_t base_weight_ = 0; // 开门前的基准重量
    
    uint32_t message_id_counter_ = 0; 
    const uint32_t FRIDGE_DEVICE_ID = 10001; 

    std::chrono::steady_clock::time_point last_static_time_;
    const std::chrono::seconds STATIC_INTERVAL{2 * 3600};

    InventoryManager inventory_manager_;

    void HandleDoorOpen();
    void HandleDoorClose();
    void CheckTimers();
    void ProcessStaticResultOnly(); 

public:
    void init();
    void run();
};