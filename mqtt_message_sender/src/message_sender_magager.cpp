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

void MessageSenderManager::EnqueueMessage(const MqttMessageStruct& message)
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_queue.push_back(message);
}

bool MessageSenderManager::SendMessage(const MqttMessageStruct& message)
{
    // 这里应该包含实际发送消息的逻辑，例如通过MQTT协议将消息发送到服务器
    // 由于这是一个示例，我们暂时不实现具体的发送逻辑
    // 可以在这里添加日志记录或其他处理逻辑
    // TODO: 集成真实MQTT发送。当前模拟为成功。
    (void)message;
    return true;
}

void MessageSenderManager::MainLoop()
{
    while (true)
    {
        // 优先发送队列中的缓存消息
        while (true) {
            MqttMessageStruct queuedMessage;
            {
                std::lock_guard<std::mutex> lock(m_dataMutex);
                if (m_queue.empty()) break;
                queuedMessage = m_queue.front();
                m_queue.pop_front();
            }

            uint8_t messageSendCount = 0; // 发送次数
            const uint8_t maxMessageSendCount = 3; // 最大发送次数
            while (true) {
                if (SendMessage(queuedMessage)) {
                    break; // 发送成功
                }
                messageSendCount++;
                if (messageSendCount >= maxMessageSendCount) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        if (IsMessageSendSwitchOn() && IsMessageSenderIdle())
        {
            SetMessageSenderStatus(SENDER_BUSY); // 设置发送器空闲状态
            m_dataMutex.lock();
            MqttMessageStruct messageToSend = m_message; // 复制消息以供发送线程使用
            m_dataMutex.unlock();
            uint8_t messageSendCount = 0; // 发送次数
            const uint8_t maxMessageSendCount = 3; // 最大发送次数
            while (true)
            {
                if (SendMessage(messageToSend))
                {
                    break; // 发送成功，退出循环
                }
                messageSendCount++;
                if (messageSendCount >= maxMessageSendCount) // 超过最大发送次数，放弃发送
                {
                    // 若发送失败且超限，可将消息入队缓存，等待后续发送
                    EnqueueMessage(messageToSend);
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