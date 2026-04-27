#include <thread>
#include "../include/message_sender_magager.h"

MessageSenderManager& MessageSenderManager::GetInstance()
{
    static MessageSenderManager instance;
    return instance;
}

MessageSenderManager::MessageSenderManager()
{
    SenderInit();
}

MessageSenderManager::~MessageSenderManager()
{
    SenderInit();
}

void MessageSenderManager::SenderInit()
{
    m_initStatus.store(INITI_UNFINISHED);
    m_SenderIdleStatus.store(SENDER_IDLE);
    m_messageSendSwitch.store(MESSAGE_SEND_SWITCH_OFF);

    m_dataMutex.lock();
    m_message = {};
    m_dataMutex.unlock();

    MessageSenderReady();
    return;
}

void MessageSenderManager::CopyMessage(const MqttMessageStruct& message)
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_message = message; // 复制消息到内部变量
}

bool MessageSenderManager::SendMessage(const MqttMessageStruct& message)
{
    // 这里应该包含实际发送消息的逻辑，例如通过MQTT协议将消息发送到服务器
    // 由于这是一个示例，我们暂时不实现具体的发送逻辑
    // 可以在这里添加日志记录或其他处理逻辑
}

void MessageSenderManager::MainLoop()
{
    while (true)
    {
        if (IsMessageSendSwitchOn() && IsMessageSenderIdle())
        {
            SetMessageSenderStatus(SENDER_BUSY); // 设置发送器空闲状态
            m_dataMutex.lock();
            MqttMessageStruct messageToSend = m_message; // 复制消息以供发送线程使用
            m_dataMutex.unlock();

            uint8_t messageSendCount = 0; // 发送次数
            uint8_t maxMessageSendCount = 3; // 最大发送次数
            while (true)
            {
                if (SendMessage(messageToSend))
                {
                    break; // 发送成功，退出循环
                }
                messageSendCount++;
                if (messageSendCount >= maxMessageSendCount) // 超过最大发送次数，放弃发送
                {
                    break;
                }
                // 可以添加适当的睡眠时间，避免CPU占用过高
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            SetMessageSendSwitch(MESSAGE_SEND_SWITCH_OFF); // 发送完成后关闭发送开关
            SetMessageSenderStatus(SENDER_IDLE); // 设置发送器空闲状态
        }
        // 可以添加适当的睡眠时间，避免CPU占用过高
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}