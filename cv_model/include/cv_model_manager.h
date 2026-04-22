#ifndef CV_MODEL_MANAGER_H
#define CV_MODEL_MANAGER_H

#include "../api/cv_model_api.h"
#include <mutex>
#include <atomic>
#include <future>

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
    bool CvModelInit();

    // CvModelManager主循环函数，负责处理识别任务和状态更新
    void MainLoop();

    // 内部核心处理逻辑(线程一次只做一个任务，避免资源竞争)
    void StaticRecognitionInternal();
    void DynamicRecognitionInternal();

    // 状态查询
    bool IsCvModelReady() const { return m_initFinished.load(); }
    bool IsStaticRecognitionBusy() const { return m_staticActive.load(); }
    bool IsDynamicRecognitionBusy() const { return m_dynamicActive.load(); }

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

    // 设置标准位，供外部查询
    void SetStaticRecognitionStatus(bool status) { m_staticActive.store(status); }
    void SetDynamicRecognitionStatus(bool status) { m_dynamicActive.store(status); }

    // 标志位使用原子变量，确保多线程读取安全
    std::atomic<bool> m_initFinished{false};
    std::atomic<bool> m_staticActive{false};
    std::atomic<bool> m_dynamicActive{false};

    // 结果数据与保护锁
    std::mutex m_dataMutex;
    StaticRecognitionResult m_staticResult;
    DynamicRecognitionResult m_dynamicResult;
};

#endif // CV_MODEL_MANAGER_H