#ifndef CAMERA_MODEL_H
#define CAMERA_MODEL_H

#include <opencv2/opencv.hpp>
#include <mutex>
#include <atomic>
#include <string>
#include <thread>

// ==================== 摄像头模块类 ====================
class CameraModule {
public:
    static CameraModule& GetInstance();

    cv::Mat GetLatestFrame();
    bool IsOpened() const { return mIsOpened.load(); }

    void SetCameraIndex(int index) { mCameraIndex = index; }
    void SetResolution(int width, int height) {
        mWidth = width;
        mHeight = height;
    }

private:
    CameraModule();
    ~CameraModule();

    CameraModule(const CameraModule&) = delete;
    CameraModule& operator=(const CameraModule&) = delete;

    void CaptureThreadFunc();
    bool OpenCamera();
    void CloseCamera();

    int mCameraIndex = 0;
    int mWidth = 640;
    int mHeight = 480;
    int mFps = 30;

    std::atomic<bool> mRunning{false};
    std::atomic<bool> mIsOpened{false};
    std::atomic<bool> mReconnectScheduled{false};

    std::mutex mFrameMutex;
    cv::Mat mLatestFrame;

    cv::VideoCapture mCapture;
    std::thread mCaptureThread;
};

#endif // CAMERA_MODEL_H