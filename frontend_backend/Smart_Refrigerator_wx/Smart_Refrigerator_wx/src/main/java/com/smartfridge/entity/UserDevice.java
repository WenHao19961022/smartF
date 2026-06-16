package com.smartfridge.entity;

import com.baomidou.mybatisplus.annotation.*;
import lombok.Data;
import java.time.LocalDateTime;

/**
 * 用户设备关联实体
 */
@Data
@TableName("user_device")
public class UserDevice {

    @TableId(type = IdType.AUTO)
    private Integer id;

    private String openid;

    private String deviceSn;

    //private String phoneNumber;

    //private String smsEnabled;

    /** 是否管理员 0-否 1-是 */
    //private Integer isAdmin;

    @TableField(fill = FieldFill.INSERT)
    private LocalDateTime createTime;
}