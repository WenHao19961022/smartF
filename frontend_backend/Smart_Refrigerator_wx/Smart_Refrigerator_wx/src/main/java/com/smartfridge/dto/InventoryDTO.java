package com.smartfridge.dto;

import lombok.*;

import java.time.LocalDateTime;

/**
 * 库存列表DTO
 */
@Setter
@Data
@AllArgsConstructor
@Getter
@NoArgsConstructor
public class InventoryDTO {
    private Integer inventoryId;
    private String fruitName;
    private String fruitCode;
    private Float weight;
    private Integer freshness;
    /** 视觉新鲜度原始值（硬件识别结果） */
    private Integer visionScore;
    /** 最终新鲜度（视觉值 × 时间衰减因子） */
    private Integer finalScore;
    private String state;
    private String expireDesc;
    private LocalDateTime entryTime;
    private LocalDateTime expireTime;
    // 该种类水果的标准保质期小时数（可选。如果没传，代码里写了自动按 7 天兜底）
    private Integer shelfLifeWhole;
    // InventoryDTO.java 中增加预警标识字段
    private Boolean isExpiring; // 是否处于临期/预警状态
    private String imageUrl;
    private String nutrient;
    private float coordinateX;

    private float coordinateY;
}