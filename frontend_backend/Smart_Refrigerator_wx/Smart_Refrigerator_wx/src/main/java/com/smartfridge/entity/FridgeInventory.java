package com.smartfridge.entity;

import com.baomidou.mybatisplus.annotation.*;
import lombok.Data;
import lombok.Getter;
import lombok.Setter;

import java.time.LocalDateTime;

/**
 * 实时库存实体
 */
@Getter
@Setter
@Data
@TableName("fridge_inventory")
public class FridgeInventory {

    @TableId(type = IdType.AUTO)
    private Long id;

    private String batchId;

    private String deviceId;

    //这个地方存的是MQTT给的每个水果的ID
    private Integer trackId;

    private String fruitName;

    private String fruitCode;

    private String freshCode;

    private Integer freshScore;

    private Float weight;

    private LocalDateTime entryTime;

    private LocalDateTime expireTime;

    private float coordinateX;

    private float coordinateY;

    @TableField(fill = FieldFill.INSERT)
    private LocalDateTime createTime;

    //因为这个表中的数据只有插入的过程，没有删除和修改的过程，所以不需要更新时间
//    @TableField(fill = FieldFill.INSERT_UPDATE)
//    private LocalDateTime updateTime;
}