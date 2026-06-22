#include "../include/camera_model.h"
#include "../../common/include/logger.h"
#include "../../common/include/config_manager.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <atomic>
#include <iomanip>

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

    LOG_PRINT("[Camera]", "Initializing Camera | Index: " << mCameraIndex
              << " | Resolution: " << mWidth << "x" << mHeight << "@" << mFps << "fps");

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

    mCapture.open(mCameraIndex, cv::CAP_V4L2);

    if (!mCapture.isOpened()) {
        LOG_PRINT("[Camera]", "Failed to open camera index " << mCameraIndex << " with V4L2 backend");
        mIsOpened.store(false);
        return false;
    }

    mCapture.set(cv::CAP_PROP_FRAME_WIDTH, mWidth);
    mCapture.set(cv::CAP_PROP_FRAME_HEIGHT, mHeight);
    mCapture.set(cv::CAP_PROP_FPS, mFps);

    int actualWidth = static_cast<int>(mCapture.get(cv::CAP_PROP_FRAME_WIDTH));
    int actualHeight = static_cast<int>(mCapture.get(cv::CAP_PROP_FRAME_HEIGHT));
    int actualFps = static_cast<int>(mCapture.get(cv::CAP_PROP_FPS));

    mIsOpened.store(true);
    LOG_PRINT("[Camera]", "Camera opened successfully | Requested: " << mWidth << "x" << mHeight << "@" << mFps
              << "fps | Actual: " << actualWidth << "x" << actualHeight << "@" << actualFps << "fps");
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
    int consecutiveFailures = 0;
    int frameCount = 0;
    auto lastStatsTime = std::chrono::steady_clock::now();

    LOG_PRINT("[Camera]", "CaptureThread started | CameraIndex: " << mCameraIndex);

    while (mRunning.load()) {
        if (!mIsOpened.load()) {
            LOG_PRINT("[Camera]", "Camera not opened, attempting to open... (retry delay: " << reconnectDelaySec << "s)");
            if (OpenCamera()) {
                reconnectDelaySec = 2;
                consecutiveFailures = 0;
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
                frameCount++;
                consecutiveFailures = 0;

                // 每10秒打印一次帧率统计
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastStatsTime).count();
                if (elapsed >= 10) {
                    double fps = static_cast<double>(frameCount) / elapsed;
                    LOG_PRINT("[Camera]", "[Stats] Captured " << frameCount << " frames in " << elapsed
                              << "s | Current FPS: " << std::fixed << std::setprecision(1) << fps
                              << " | Frame size: " << tempFrame.cols << "x" << tempFrame.rows
                              << " | Type: " << tempFrame.type());
                    frameCount = 0;
                    lastStatsTime = now;
                }
            } else {
                consecutiveFailures++;
                if (consecutiveFailures <= 3) {
                    LOG_PRINT("[Camera]", "Empty frame captured (consecutive: " << consecutiveFailures << ")");
                }
            }
        } else {
            consecutiveFailures++;
            LOG_PRINT("[Camera]", "Frame read() failed, scheduling reconnect... (consecutive failures: " << consecutiveFailures << ")");
            mIsOpened.store(false);
            CloseCamera();

            std::this_thread::sleep_for(std::chrono::seconds(reconnectDelaySec));
            reconnectDelaySec = std::min(reconnectDelaySec * 2, maxReconnectDelaySec);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    LOG_PRINT("[Camera]", "CaptureThread exiting...");
}

cv::Mat CameraModule::GetLatestFrame() {
    std::lock_guard<std::mutex> lock(mFrameMutex);
    if (mLatestFrame.empty()) {
        return cv::Mat();
    }
    return mLatestFrame.clone();
}
