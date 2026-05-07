#ifndef CAMERA_MODEL_H
#define CAMERA_MODEL_H

#include <opencv2/opencv.hpp>
#include <mutex>
#include <atomic>

class CameraModule
{
public:
    static CameraModule& GetInstance();

    // 获取最新的图像帧，供识别模块调用
    cv::Mat GetLatestFrame();
private:
    CameraModule();
    ~CameraModule();
    // 禁止拷贝与赋值
    CameraModule(const CameraModule&) = delete;
    CameraModule& operator=(const CameraModule&) = delete;

    // 这里可以添加成员变量，例如摄像头设备、图像缓冲区等
    std::mutex m_frameMutex; // 保护图像帧的互斥
    cv::Mat m_latestFrame; // 存储最新的图像帧
};

#endif // CAMERA_MODEL_H