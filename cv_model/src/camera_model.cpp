#include "../include/camera_model.h"
#include <thread>
#include <chrono>
#include <iostream>

CameraModule& CameraModule::GetInstance() {
    static CameraModule instance;
    return instance;
}

CameraModule::CameraModule() {
    m_running.store(true);
    m_isOpened.store(false);

    // 启动采集线程
    m_captureThread = std::thread(&CameraModule::captureThreadFunc, this);
}

CameraModule::~CameraModule() {
    m_running.store(false);

    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }

    closeCamera();
}

bool CameraModule::openCamera() {
    closeCamera();

    m_capture.open(m_cameraIndex);

    if (!m_capture.isOpened()) {
        std::cerr << "[Camera] Failed to open camera index " << m_cameraIndex << std::endl;
        m_isOpened.store(false);
        return false;
    }

    // 设置分辨率
    m_capture.set(cv::CAP_PROP_FRAME_WIDTH, m_width);
    m_capture.set(cv::CAP_PROP_FRAME_HEIGHT, m_height);

    // 设置帧率
    m_capture.set(cv::CAP_PROP_FPS, 30);

    m_isOpened.store(true);
    std::cout << "[Camera] Camera opened successfully ("
              << m_width << "x" << m_height << ")" << std::endl;
    return true;
}

void CameraModule::closeCamera() {
    if (m_capture.isOpened()) {
        m_capture.release();
    }
    m_isOpened.store(false);
}

void CameraModule::captureThreadFunc() {
    int reconnectDelaySec = 2;
    int maxReconnectDelaySec = 30;

    while (m_running.load()) {
        if (!m_isOpened.load()) {
            if (openCamera()) {
                reconnectDelaySec = 2;
            } else {
                std::this_thread::sleep_for(std::chrono::seconds(reconnectDelaySec));
                reconnectDelaySec = std::min(reconnectDelaySec * 2, maxReconnectDelaySec);
            }
            continue;
        }

        cv::Mat tempFrame;
        if (m_capture.read(tempFrame)) {
            if (!tempFrame.empty()) {
                std::lock_guard<std::mutex> lock(m_frameMutex);
                tempFrame.copyTo(m_latestFrame);
            }
        } else {
            // 读取失败，摄像头可能断开
            std::cerr << "[Camera] Frame capture failed, scheduling reconnect..." << std::endl;
            m_isOpened.store(false);
            closeCamera();

            std::this_thread::sleep_for(std::chrono::seconds(reconnectDelaySec));
            reconnectDelaySec = std::min(reconnectDelaySec * 2, maxReconnectDelaySec);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

cv::Mat CameraModule::GetLatestFrame() {
    std::lock_guard<std::mutex> lock(m_frameMutex);

    if (m_latestFrame.empty()) {
        return cv::Mat();
    }

    return m_latestFrame.clone();
}