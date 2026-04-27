#ifndef CV_MODEL_MANAGER_H
#define CV_MODEL_MANAGER_H

#include "../api/cv_model_api.h"
#include <mutex>
#include <atomic>
#include <future>

const static bool INITI_FINISHED = true; // 定义一个常量，表示初始化是否完成
const static bool INITI_UNFINISHED = false; // 定义一个常量，表示初始化是否未完成
const static bool RECOGNITION_SWITCH_ON = true; // 定义一个常量，表示检测是否激活
const static bool RECOGNITION_SWITCH_OFF = false; // 定义一个常量，表示检测是否不激活
const static bool RECOGNITION_IDLE = true; // 定义一个常量，表示识别线程空闲
const static bool RECOGNITION_BUSY = false; // 定义一个常量，表示识别是否正在进行中

/**
 * @brief 计算机视觉模型管理类 (单例)
 * 针对 Jetson Xavier 优化：增加线程安全保护与异步任务管理
 */
class CvModelManager
{
public:
    /**
     * @brief 获取单例实例 (C++11 局部静态变量方式，线程安全)
     */
    static CvModelManager& GetInstance();

    // 模型初始化（建议在程序启动时显式调用一次）
    void CvModelInit();

    // CvModelManager主循环函数，负责处理识别任务和状态更新
    void MainLoop();

    // 状态查询
    bool IsCvModelReady() const { return m_initStatus.load(); }
    bool IsStaticRecognitionIdle() const { return m_staticRecognitionStatus.load(); }
    bool IsDynamicRecognitionIdle() const { return m_dynamicecognitionStatus.load(); }
    void SetStaticRecognitionSwitch(bool active) { m_staticRecognitionSwitch.store(active); }
    void SetDynamicRecognitionSwitch(bool active) { m_dynamicecognitionSwitch.store(active); }

    // 结果获取 (增加互斥锁保护数据一致性)
    StaticRecognitionResult GetStaticResult();
    DynamicRecognitionResult GetDynamicResult();

private:
    // 构造/析构私有化
    CvModelManager();
    ~CvModelManager();

    // 禁止拷贝与赋值
    CvModelManager(const CvModelManager&) = delete;
    CvModelManager& operator=(const CvModelManager&) = delete;

    // 内部核心处理逻辑(线程一次只做一个任务，避免资源竞争)
    void StaticRecognitionInternal();
    void DynamicRecognitionInternal();

    // 设置标准位，供与core通讯
    bool IsStaticRecognitionSwitchOn() const { return m_staticRecognitionSwitch.load(); }
    bool IsDynamicRecognitionSwitchOn() const { return m_dynamicecognitionSwitch.load(); }
    void SetStaticRecognitionStatus(bool status) { m_staticRecognitionStatus.store(status); }
    void SetDynamicRecognitionStatus(bool status) { m_dynamicecognitionStatus.store(status); }

    // 标志位使用原子变量，确保多线程读取安全
    std::atomic<bool> m_initStatus{INITI_UNFINISHED};
    std::atomic<bool> m_staticRecognitionSwitch{RECOGNITION_SWITCH_OFF};
    std::atomic<bool> m_dynamicecognitionSwitch{RECOGNITION_SWITCH_OFF};
    std::atomic<bool> m_staticRecognitionStatus{RECOGNITION_IDLE};
    std::atomic<bool> m_dynamicecognitionStatus{RECOGNITION_IDLE};

    void CvModelReady() { m_initStatus.store(INITI_FINISHED); }

    // 结果数据与保护锁
    std::mutex m_dataMutex;
    StaticRecognitionResult m_staticRecognitionResult;
    DynamicRecognitionResult m_dynamicRecognitionResult;
};

#endif // CV_MODEL_MANAGER_H