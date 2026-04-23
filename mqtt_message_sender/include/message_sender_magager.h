#ifndef MESSAGE_SENDER_MANAGER_H
#define MESSAGE_SENDER_MANAGER_H

#include <mutex>
#include <atomic> // 包含atomic头文件以使用原子变量
#include "../api/mqtt_message_sender_api.h" // 包含mqtt_message_sender_api.h头文件以使用其中定义的结构体和函数声明

class MessageSenderManager
{
public:
    // 获取MessageSenderManager实例的静态方法
    static MessageSenderManager& getInstance();

    void Init(); // 初始化方法，建议在程序启动时显式调用一次

    bool IsSenderReady() const { return m_initFinished.load(); } // 检查发送器是否准备就绪

    // 设置发送标志位，供与core通讯
    void setSendFlag(bool flag) { m_sendActive.store(flag); }

    // 发送功能主循环
    void MainLoop();

private:
    // 私有构造函数，防止外部实例化
    MessageSenderManager();
    // 析构函数
    ~MessageSenderManager();
    // 禁止复制构造函数和赋值运算符，确保单例模式
    MessageSenderManager(const MessageSenderManager&) = delete;
    MessageSenderManager& operator=(const MessageSenderManager&) = delete;

    // 发送消息的方法，接受一个MqttMessageStruct类型的参数
    bool sendMessage(const MqttMessageStruct& message);

    // 标志位使用原子变量，确保多线程读取安全
    std::atomic<bool> m_initFinished{false};
    std::atomic<bool> m_sendActive{false};

    //发送的信息
    std::mutex m_dataMutex;
    MqttMessageStruct m_message;
};


#endif // MESSAGE_SENDER_MANAGER_H