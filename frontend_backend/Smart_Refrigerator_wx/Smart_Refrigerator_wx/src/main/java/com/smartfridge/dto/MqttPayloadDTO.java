package com.smartfridge.dto;

import com.fasterxml.jackson.annotation.JsonProperty;
import lombok.Data;
import java.util.List;

/**
 * 硬件全量快照上报 MQTT 数据 DTO
 * 适配 JSON 格式：包含设备信息、温湿度以及水果对象列表
 */
@Data
public class MqttPayloadDTO {

    /** 消息类型：startup 表示设备刚启动，snapshot 表示库存快照。 */
    @JsonProperty("event_type")
    private String eventType;

    /** 0=正常，其余值表示边缘端拒绝盘点的原因。 */
    @JsonProperty("recognition_status")
    private Integer recognitionStatus;

    /** 设备唯一序列号 (对应 JSON 的 device_id) */
    @JsonProperty("device_id")
    private String deviceId;

    /** 消息唯一ID，用于后端幂等性去重 (对应 JSON 的 message_id) */
    @JsonProperty("message_id")
    private String messageId;

    /** 消息产生的时间戳 (对应 JSON 的 message_time) */
    @JsonProperty("message_time")
    private String messageTime;

    /** 冰箱环境信息 (温度、湿度) */
    @JsonProperty("refrigerator_info")
    private RefrigeratorInfoDTO refrigeratorInfo;

    /** 水果总量统计 */
    @JsonProperty("fruit_num")
    private Integer fruitNum;

    /** 冰箱内当前所有水果的明细列表 */
    @JsonProperty("fruits")
    private List<FruitItemDTO> fruits;

    /**
     * 内部类：环境信息
     */
    @Data
    public static class RefrigeratorInfoDTO {
        /** 实时温度(℃) */
        private Float temperature;
        /** 实时湿度(%) */
        private Float humidity;
    }

    /**
     * 内部类：水果个体明细
     */
    @Data
    public static class FruitItemDTO {
        /**
         * 硬件算法生成的追踪ID
         * 重要：用于区分不同的个体（例如同时有两个苹果，ID分别为101, 102）
         */
        private Integer id;

        /** 视觉识别的水果代码 (对应字典表的 category_code，如 apple) */
        private String type;

        /**
         * 视觉算法评估的新鲜度标签 (fresh_xxx / rotten_xxx)
         * 例如: fresh_apple, rotten_banana
         */
        @JsonProperty("fresh_status")
        private String freshStatus;

        /**
         * 当前重量(g)
         * 现阶段预留口子，逻辑层暂不强制处理
         */
        private Float weight;

        /**
         * 硬件记录的首次放入时间，格式 yyyy-MM-dd HH:mm:ss
         */
        @JsonProperty("put_in_time")
        private String putInTime;

        /**
         * 水果的位置X
         */
        @JsonProperty("coordinate_x")
        private float coordinateX;

        /**
         * 水果的位置Y
         */
        @JsonProperty("coordinate_y")
        private float coordinateY;
    }
}
