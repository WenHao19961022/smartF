#include "../include/message_sender_magager.h" // 包含message_sender_magager.h头文件以使用MessageSenderManager类的定义
#include "../api/mqtt_message_sender_api.h" // 包含mqtt_message_sender_api.h头文件以使用其中定义的结构体和函数声明

bool SendMqttMessage(const MqttMessageStruct& message)
{
    if (!(MessageSenderManager::GetInstance().IsSenderReady())) {
        return false; // 发送器未准备好，无法发送消息
    }

    if (MessageSenderManager::GetInstance().IsMessageSenderIdle()) {
        // 拷贝发送的信息
        MessageSenderManager::GetInstance().CopyMessage(message);
        // 设置发送标志位，通知发送线程可以发送消息了
        MessageSenderManager::GetInstance().SetMessageSendSwitch(MESSAGE_SEND_SWITCH_ON);
        return true; // 或者可以添加等待机制，直到message_sender准备好
    }
    return false;
}