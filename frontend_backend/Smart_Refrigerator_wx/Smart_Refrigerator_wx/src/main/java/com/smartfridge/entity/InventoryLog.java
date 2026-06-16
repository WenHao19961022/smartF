package com.smartfridge.entity;

import com.baomidou.mybatisplus.annotation.*;
import lombok.Data;
import java.time.LocalDateTime;

/**
 * 重量变化流水实体
 */
@Data
@TableName("inventory_log")
public class InventoryLog {

    @TableId(type = IdType.AUTO)
    private Long id;

    private Long inventoryId;

    private String deviceId;

    private Float actionType;

    private Float freshCore;

    private Float weight;

    //private String msgId;

    private LocalDateTime logTime;

    @TableField(fill = FieldFill.INSERT)
    private LocalDateTime createTime;
}