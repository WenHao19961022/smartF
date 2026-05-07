#include "../include/camera_model.h"
#include <thread>
#include <iostream>

// 获取单例实例
CameraModule& CameraModule::GetInstance() {
    static CameraModule instance;
    return instance;
}

// 构造函数：初始化摄像头并开启采集线程
CameraModule::CameraModule() {
    // 启动一个后台线程来更新 m_latestFrame
    // 在实际生产代码中，建议将线程对象保存为成员变量以便优雅停止
    std::thread([this]() {
        cv::VideoCapture cap(0); // 打开默认摄像头
        if (!cap.isOpened()) {
            std::cerr << "错误：无法打开摄像头！" << std::endl;
            return;
        }

        cv::Mat tempFrame;
        while (true) {
            if (cap.read(tempFrame)) {
                // 使用互斥锁保护共享资源
                std::lock_guard<std::mutex> lock(m_frameMutex);
                tempFrame.copyTo(m_latestFrame);
            }
            // 适当休眠以降低 CPU 占用（例如对应 30 FPS）
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }).detach(); // 这里使用 detach 简化演示，实际项目中建议管理好线程生命周期
}

CameraModule::~CameraModule() {
    // 如果有资源（如摄像头句柄）需要释放，在此处理
}

// 获取最新的图像帧
cv::Mat CameraModule::GetLatestFrame() {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    
    // 如果没有获取到有效帧，返回空矩阵
    if (m_latestFrame.empty()) {
        return cv::Mat();
    }

    // 返回副本以确保调用者在处理时，后台线程可以安全地更新原始矩阵
    return m_latestFrame.clone();
}