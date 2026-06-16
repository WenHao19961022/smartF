package com.smartfridge.entity;

import com.baomidou.mybatisplus.annotation.*;
import lombok.Data;

import java.time.LocalDateTime;

/**
 * 设备状态实体
 */
@Data
@TableName("device_status")
public class DeviceStatus {

    @TableId(type = IdType.AUTO)
    private Integer id;

    private String deviceSn;

    /** 0-离线, 1-在线 */
    private Integer online;

    private Float temperature;

    private Float humidity;

    private String batchId;

    @TableField(fill = FieldFill.INSERT)
    private LocalDateTime createTime;

    @TableField(fill = FieldFill.INSERT_UPDATE)
    private LocalDateTime updateTime;
}