package com.smartfridge.mqtt;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.smartfridge.dto.MqttPayloadDTO;
import com.smartfridge.service.DeviceService;
import com.smartfridge.service.InventoryService;
import com.smartfridge.service.RedisService;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.integration.annotation.ServiceActivator;
import org.springframework.messaging.Message;
import org.springframework.stereotype.Component;
import org.springframework.transaction.annotation.Transactional;

/**
 * MQTT消息处理器 (全量快照版)
 * 采用"只增不减"策略：每一包消息都视为该时刻冰箱的完整状态快照，直接存入数据库
 * 就是无脑插入，来一批插入一批，避免复杂数据库修改操作
 *
 * 关键设计：
 * 1. 幂等去重：基于 Redis SETNX 按 deviceSn+msgId 原子认领
 * 2. 降级处理：Redis 不可用时跳过去重，append-only 容忍重复
 * 3. 事务边界：设备状态更新 + 库存快照写入同一事务，保证状态与数据一致
 * 4. 失败回滚：DB 写入失败时主动释放 Redis 标记，让 MQTT 重传可被处理
 */
@Slf4j
@Component
public class MqttMessageHandler {

    @Autowired
    private ObjectMapper objectMapper;

    @Autowired
    private InventoryService inventoryService;

    @Autowired
    private DeviceService deviceService;

    @Autowired
    private RedisService redisService;

    /**
     * 处理MQTT入站消息
     */
    @ServiceActivator(inputChannel = "mqttInputChannel")
    @Transactional(rollbackFor = Exception.class)
    public void handleMessage(Message<?> message) {
        String payload = String.valueOf(message.getPayload());
        log.info("收到硬件快照消息: {}", payload);

        MqttPayloadDTO data = null;
        boolean redisClaimed = false;
        try {
            // 1. 将 JSON 解析为 DTO
            data = objectMapper.readValue(payload, MqttPayloadDTO.class);

            // 2. 基础参数校验 (deviceId 是绑定用户的关键)
            if (data.getDeviceId() == null || data.getMessageId() == null) {
                log.warn("MQTT消息格式非法，缺少 device_id 或 message_id");
                return;
            }

            // 3. 幂等去重（基于 SETNX 原子操作）
            // 正常路径：Redis SETNX 原子认领
            // 降级路径：Redis 异常时跳过去重（append-only 策略可容忍少量重复）
            try {
                if (!redisService.isMqttMessageProcessed(data.getDeviceId(), data.getMessageId())) {
                    log.info("检测到重复MQTT消息，跳过: deviceId={}, msgId={}",
                            data.getDeviceId(), data.getMessageId());
                    return;
                }
                redisClaimed = true;
            } catch (Exception redisEx) {
                log.warn("Redis 去重不可用，降级为不验重: deviceId={}, msgId={}, err={}",
                        data.getDeviceId(), data.getMessageId(), redisEx.getMessage());
            }

            // 4. 更新设备运行状态 (温湿度、在线状态)
            // 与后续快照写入在同一事务中，事务回滚则状态一起回滚
            if (data.getRefrigeratorInfo() != null) {
                deviceService.updateDeviceStatus(
                        data.getDeviceId(),
                        1, // 状态：在线
                        data.getRefrigeratorInfo().getTemperature(),
                        data.getRefrigeratorInfo().getHumidity(),
                        data.getMessageId()
                );
            }

            // 启动事件只刷新设备在线/重启时间，不应生成一批空库存快照。
            if ("startup".equalsIgnoreCase(data.getEventType())) {
                log.info("收到设备启动通知: deviceId={}, msgId={}, startupTime={}",
                        data.getDeviceId(), data.getMessageId(), data.getMessageTime());
                return;
            }

            // 5. 执行全量插入逻辑 (只增不减策略)
            // 逻辑由 InventoryService 内部处理：将 fruits 列表中的每项存为新记录
            inventoryService.saveInventorySnapshot(data);

            log.info("快照处理完成: deviceId={}, batchId={}, count={}",
                    data.getDeviceId(), data.getMessageId(), data.getFruitNum());

        } catch (Exception e) {
            // DB 事务回滚（@Transactional 已配置 rollbackFor=Exception）
            // 主动释放 Redis 去重标记，让 MQTT 重传的消息能再次被处理，避免永久丢消息
            if (redisClaimed && data != null) {
                try {
                    redisService.releaseMqttMessageProcessed(data.getDeviceId(), data.getMessageId());
                } catch (Exception cleanupEx) {
                    log.error("释放 Redis 去重标记失败，将导致1小时内该消息被丢弃: deviceId={}, msgId={}",
                            data.getDeviceId(), data.getMessageId(), cleanupEx);
                }
            }
            log.error("解析或存储硬件快照失败: {}", payload, e);
        }
    }
}
