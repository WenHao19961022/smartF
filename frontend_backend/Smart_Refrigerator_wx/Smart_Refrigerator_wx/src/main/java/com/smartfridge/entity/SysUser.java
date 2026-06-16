package com.smartfridge.entity;

import com.baomidou.mybatisplus.annotation.*;
import lombok.Data;

import java.time.LocalDateTime;

/**
 * 系统用户表 - 对接微信小程序用户
 */
@Data
@TableName("sys_user")
public class SysUser {

    @TableId(type = IdType.AUTO)
    private Long id;

    /** 微信唯一标识 */
    private String openid;

    /** 用户昵称 */
    private String nickname;

    /** 头像URL */
    private String avatar;

    /** 是否开启推送通知 0-关闭 1-开启 */
    private Integer pushEnabled;

    /** 手机号(用于短信提醒) */
    //private String phoneNumber;

    /** 短信 */
    private String email;

    /** 是否开启短信提醒：1为通知；0位未通知 */
    private Integer smsEnabled;

    @TableField(fill = FieldFill.INSERT)
    private LocalDateTime createTime;

    @TableField(fill = FieldFill.INSERT_UPDATE)
    private LocalDateTime updateTime;

    public boolean getPushEnabled() {
        return pushEnabled != null && pushEnabled == 1;
    }

    public boolean getSmsEnabled() {
        return smsEnabled != null && smsEnabled == 1;
    }
}
