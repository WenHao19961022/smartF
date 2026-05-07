#ifndef CAMERA_MODEL_H
#define CAMERA_MODEL_H

#include <opencv2/opencv.hpp>
#include <mutex>
#include <atomic>
#include <string>
#include <thread>

class CameraModule {
public:
    static CameraModule& GetInstance();

    // 获取最新的图像帧
    cv::Mat GetLatestFrame();

    // 摄像头状态查询
    bool isOpened() const { return m_isOpened.load(); }

    // 配置接口
    void setCameraIndex(int index) { m_cameraIndex = index; }
    void setResolution(int width, int height) {
        m_width = width;
        m_height = height;
    }

private:
    CameraModule();
    ~CameraModule();

    CameraModule(const CameraModule&) = delete;
    CameraModule& operator=(const CameraModule&) = delete;

    void captureThreadFunc();
    bool openCamera();
    void closeCamera();

    int m_cameraIndex = 0;
    int m_width = 640;
    int m_height = 480;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_isOpened{false};
    std::atomic<bool> m_reconnectScheduled{false};

    std::mutex m_frameMutex;
    cv::Mat m_latestFrame;

    cv::VideoCapture m_capture;
    std::thread m_captureThread;
};

#endif // CAMERA_MODEL_H