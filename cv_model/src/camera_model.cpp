#include "../include/camera_model.h"
#include "../../common/include/logger.h"
#include "../../common/include/config_manager.h"
#include <thread>
#include <chrono>
#include <iostream>

CameraModule& CameraModule::GetInstance() {
    static CameraModule instance;
    return instance;
}

CameraModule::CameraModule() {
    // 从配置管理器读取摄像头参数
    mCameraIndex = ConfigManager::GetInstance().GetInt("camera.index", 0);
    mWidth = ConfigManager::GetInstance().GetInt("camera.width", 640);
    mHeight = ConfigManager::GetInstance().GetInt("camera.height", 480);
    mFps = ConfigManager::GetInstance().GetInt("camera.fps", 30);

    mRunning.store(true);
    mIsOpened.store(false);
    mCaptureThread = std::thread(&CameraModule::CaptureThreadFunc, this);
}

CameraModule::~CameraModule() {
    mRunning.store(false);
    if (mCaptureThread.joinable()) {
        mCaptureThread.join();
    }
    CloseCamera();
}

bool CameraModule::OpenCamera() {
    CloseCamera();

    mCapture.open(mCameraIndex);

    if (!mCapture.isOpened()) {
        LOG_PRINT("[Camera]", "Failed to open camera index " << mCameraIndex);
        mIsOpened.store(false);
        return false;
    }

    mCapture.set(cv::CAP_PROP_FRAME_WIDTH, mWidth);
    mCapture.set(cv::CAP_PROP_FRAME_HEIGHT, mHeight);
    mCapture.set(cv::CAP_PROP_FPS, mFps);

    mIsOpened.store(true);
    LOG_PRINT("[Camera]", "Camera opened successfully (" << mWidth << "x" << mHeight << "@" << mFps << "fps)");
    return true;
}

void CameraModule::CloseCamera() {
    if (mCapture.isOpened()) {
        mCapture.release();
    }
    mIsOpened.store(false);
}

void CameraModule::CaptureThreadFunc() {
    int reconnectDelaySec = 2;
    int maxReconnectDelaySec = 30;

    while (mRunning.load()) {
        if (!mIsOpened.load()) {
            if (OpenCamera()) {
                reconnectDelaySec = 2;
            } else {
                std::this_thread::sleep_for(std::chrono::seconds(reconnectDelaySec));
                reconnectDelaySec = std::min(reconnectDelaySec * 2, maxReconnectDelaySec);
            }
            continue;
        }

        cv::Mat tempFrame;
        if (mCapture.read(tempFrame)) {
            if (!tempFrame.empty()) {
                std::lock_guard<std::mutex> lock(mFrameMutex);
                tempFrame.copyTo(mLatestFrame);
            }
        } else {
            LOG_PRINT("[Camera]", "Frame capture failed, scheduling reconnect...");
            mIsOpened.store(false);
            CloseCamera();

            std::this_thread::sleep_for(std::chrono::seconds(reconnectDelaySec));
            reconnectDelaySec = std::min(reconnectDelaySec * 2, maxReconnectDelaySec);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

cv::Mat CameraModule::GetLatestFrame() {
    std::lock_guard<std::mutex> lock(mFrameMutex);
    if (mLatestFrame.empty()) {
        return cv::Mat();
    }
    return mLatestFrame.clone();
}
