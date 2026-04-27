#include <thread>
#include "../include/message_sender_magager.h" // 包含message_sender_magager.h头文件以使用MessageSenderManager类的定义
#include "../api/mqtt_message_sender_api.h" // 包含mqtt_message_sender_api.h头文件以使用其中定义的结构体和函数声明

bool SendMqttMessage(const MqttMessageStruct& message)
{
    if (!(MessageSenderManager::GetInstance().IsSenderReady())) {
        return false; // 发送器未准备好，无法发送消息
    }

    uint8_t retryCount = 0; // 定义重试次数
    const uint8_t maxRetryCount = 3; // 定义最大重试次数

    while (true)
    {
        if (MessageSenderManager::GetInstance().IsMessageSenderIdle()) {
            // 拷贝发送的信息
            MessageSenderManager::GetInstance().CopyMessage(message);
            // 设置发送标志位，通知发送线程可以发送消息了
            MessageSenderManager::GetInstance().SetMessageSendSwitch(MESSAGE_SEND_SWITCH_ON);
            return true; // 或者可以添加等待机制，直到message_sender准备好
        }
        retryCount++;
        if (retryCount >= maxRetryCount) {
            return false; // 超过最大重试次数，放弃发送
        }
        // 可以添加适当的睡眠时间，避免CPU占用过高
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

void MqttMessageSenderMainLoop()
{
    // 这里应该包含实际处理发送MQTT消息的逻辑，例如定时发送消息或根据特定事件触发发送消息
    // 由于这是一个示例，我们暂时调用MessageSenderManager的MainLoop方法来处理发送消息的逻辑
    MessageSenderManager::GetInstance().MainLoop();
}