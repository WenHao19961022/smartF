#include <thread>
#include "../include/message_sender_magager.h"
#include "../api/mqtt_message_sender_api.h"
#include "../../common/include/logger.h"

bool SendMqttMessage(const MqttMessageStruct& message) {
    if (!(MessageSenderManager::GetInstance().IsReady())) {
        LOG_PRINT("[Mqtt]", "SendMqttMessage: Sender NOT ready, message dropped!");
        return false; // 发送器未准备好，无法发送消息
    }

    LOG_PRINT("[Mqtt]", "SendMqttMessage: entering (msgId=" << message.messageId
              << " fruitCount=" << (int)message.fruitCount << ")");

    uint8_t retryCount = 0;
    const uint8_t maxRetryCount = 3;

    while (true) {
        if (MessageSenderManager::GetInstance().IsIdle()) {
            // 拷贝发送的信息
            MessageSenderManager::GetInstance().CopyMessage(message);
            // 设置发送标志位，通知发送线程可以发送消息了
            MessageSenderManager::GetInstance().SetMessageSendSwitch(kMessageSendSwitchOn);
            LOG_PRINT("[Mqtt]", "SendMqttMessage: message copied and switch set ON (retries=" << (int)retryCount << ")");
            return true;
        }
        retryCount++;
        if (retryCount >= maxRetryCount) {
            LOG_PRINT("[Mqtt]", "SendMqttMessage: sender busy after " << (int)maxRetryCount << " retries, message dropped!");
            return false; // 超过最大重试次数，放弃发送
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

void MqttMessageSenderMainLoop() {
    MessageSenderManager::GetInstance().MainLoop();
}