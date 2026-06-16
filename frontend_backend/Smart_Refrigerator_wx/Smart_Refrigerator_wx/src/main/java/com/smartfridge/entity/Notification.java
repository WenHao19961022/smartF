package com.smartfridge.entity;

import com.baomidou.mybatisplus.annotation.*;
import lombok.Data;
import java.time.LocalDateTime;

/**
 * 消息通知实体
 */
@Data
@TableName("notification")
public class Notification {

    @TableId(type = IdType.AUTO)
    private Long id;

    private String openid;

    private String deviceSn;

    /** 类型: warning, info, alert */
    private String type;

    private String title;

    private String content;

    /** 0-未读, 1-已读 */
    private Integer isRead;

    @TableField(fill = FieldFill.INSERT)
    private LocalDateTime createTime;
}