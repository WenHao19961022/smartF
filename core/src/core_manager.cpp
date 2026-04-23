#include <iostream>
#include <chrono>
#include "safe_queue.h"
#include "core_events.h"

// 声明全局的通信队列（实际项目中建议封装在单例或Context类中）
extern SafeQueue<CoreEvent> g_core_queue;
extern SafeQueue<CvCommand> g_cv_queue;
extern SafeQueue<std::pair<ServerCommand, std::string>> g_server_queue;

class CoreManager {
private:
    bool running_ = true;
    bool is_door_open_ = false;
    
    // 记录上一次进行静态检测的时间
    std::chrono::steady_clock::time_point last_static_time_;
    // 定时触发的间隔（例如：每 2 小时触发一次）
    const std::chrono::seconds STATIC_INTERVAL{2 * 3600}; 

public:
    void init() {
        last_static_time_ = std::chrono::steady_clock::now();
        std::cout << "[Core] Core Manager 初始化完成." << std::endl;
    }

    void run() {
        while (running_) {
            CoreEvent event;
            // 等待事件，最多等待 1000ms。超时则去检查是否需要执行定时任务
            if (g_core_queue.pop_with_timeout(event, 1000)) {
                handleEvent(event);
            } else {
                checkTimers();
            }
        }
    }

private:
    void handleEvent(const CoreEvent& event) {
        switch (event.type) {
            case CoreEventType::STM32_DOOR_OPEN:
                std::cout << "[Core] 收到开门事件，启动动态检测！" << std::endl;
                is_door_open_ = true;
                g_cv_queue.push(CvCommand::START_DYNAMIC);
                break;

            case CoreEventType::STM32_DOOR_CLOSE:
                std::cout << "[Core] 收到关门事件，停止动态检测，并立即触发静态检测！" << std::endl;
                is_door_open_ = false;
                g_cv_queue.push(CvCommand::STOP_DYNAMIC); // CV收到后会结算出 DYNAMIC_RESULT 发给 Core
                g_cv_queue.push(CvCommand::DO_STATIC);    // 紧接着让 CV 拍高清大图进行全盘静态检测
                last_static_time_ = std::chrono::steady_clock::now(); // 重置定时器
                break;

            case CoreEventType::CV_DYNAMIC_RESULT:
                std::cout << "[Core] 收到动态检测结果，准备上传变动数据..." << std::endl;
                // 将刚放进去或拿出来的增减量数据交给 Server 线程
                g_server_queue.push({ServerCommand::UPLOAD_DYNAMIC_CHANGES, event.payload});
                break;

            case CoreEventType::CV_STATIC_RESULT:
                std::cout << "[Core] 收到静态检测结果，准备上传全盘及腐败数据..." << std::endl;
                // 将全冰箱的水果状态及腐败情况交给 Server 线程
                g_server_queue.push({ServerCommand::UPLOAD_STATIC_FULL, event.payload});
                break;
        }
    }

    void checkTimers() {
        // 如果门是关着的，且距离上次静态检测超过了设定的时间
        if (!is_door_open_) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_static_time_) >= STATIC_INTERVAL) {
                std::cout << "[Core] 定时器触发，执行例行静态检测！" << std::endl;
                g_cv_queue.push(CvCommand::DO_STATIC);
                last_static_time_ = now;
            }
        }
    }
};