#ifndef MESSAGE_SENDER_MANAGER_H
#define MESSAGE_SENDER_MANAGER_H

#include <mutex>
#include <atomic> // 包含atomic头文件以使用原子变量
#include "../api/mqtt_message_sender_api.h" // 包含mqtt_message_sender_api.h头文件以使用其中定义的结构体和函数声明

const static bool INITI_FINISHED = true; // 定义一个常量，表示初始化是否完成
const static bool INITI_UNFINISHED = false; // 定义一个常量，表示初始化是否未完成
const static bool SENDER_IDLE = true; // 定义一个常量，表示发送器是否空闲
const static bool SENDER_BUSY = false; // 定义一个常量，表示发送器是否正在使用
const static bool MESSAGE_SEND_SWITCH_ON = true; // 定义一个常量，表示发送开关是否开启
const static bool MESSAGE_SEND_SWITCH_OFF = false; // 定义一个常量，表示发送开关是否关闭

class MessageSenderManager
{
public:
    // 获取MessageSenderManager实例的静态方法
    static MessageSenderManager& GetInstance();

    void SenderInit(); // 初始化方法，建议在程序启动时显式调用一次

    bool IsSenderReady() const { return m_initStatus.load(); } // 检查发送器是否准备就绪
    bool IsMessageSenderIdle() const { return m_SenderIdleStatus.load(); } // 检查发送器是否空闲，供与core通讯
    void CopyMessage(const MqttMessageStruct& message); // 复制core带过来的消息到message_sender内部，供发送线程使用
    void SetMessageSendSwitch(bool flag) { m_messageSendSwitch.store(flag); } // 设置发送开关，供与core通讯

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
    bool SendMessage(const MqttMessageStruct& message);

    void MessageSenderReady() { m_initStatus.store(INITI_FINISHED); } // 设置发送器准备就绪的标志位
    bool IsMessageSendSwitchOn() const { return m_messageSendSwitch.load(); } // 检查发送开关状态
    void SetMessageSenderStatus(bool idle) { m_SenderIdleStatus.store(idle); } // 设置发送器空闲状态的标志位

    // 标志位使用原子变量，确保多线程读取安全
    std::atomic<bool> m_initStatus{INITI_UNFINISHED};
    std::atomic<bool> m_SenderIdleStatus{SENDER_IDLE};
    std::atomic<bool> m_messageSendSwitch{MESSAGE_SEND_SWITCH_OFF};

    //发送的信息
    std::mutex m_dataMutex;
    MqttMessageStruct m_message;
};

#endif // MESSAGE_SENDER_MANAGER_H