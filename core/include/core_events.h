#pragma once
#include <string>

// Core 接收到的事件类型
enum class CoreEventType {
    STM32_DOOR_OPEN,       // STM32检测到开门
    STM32_DOOR_CLOSE,      // STM32检测到关门
    CV_DYNAMIC_RESULT,     // CV传回的增减变化数据
    CV_STATIC_RESULT       // CV传回的冰箱全量/腐败数据
};

struct CoreEvent {
    CoreEventType type;
    std::string payload;   // 携带的数据（可以是JSON或序列化结构体）
};

// CV 模块接收的指令类型
enum class CvCommand {
    START_DYNAMIC,
    STOP_DYNAMIC,
    DO_STATIC
};

// Server 模块接收的指令类型
enum class ServerCommand {
    UPLOAD_DYNAMIC_CHANGES,
    UPLOAD_STATIC_FULL
};