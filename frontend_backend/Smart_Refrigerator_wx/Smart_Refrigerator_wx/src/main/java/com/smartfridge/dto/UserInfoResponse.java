package com.smartfridge.dto;

import io.swagger.v3.oas.annotations.media.Schema;
import lombok.Data;

/**
 * 用户信息响应
 */
@Data
@Schema(description = "用户信息响应")
public class UserInfoResponse {

    @Schema(description = "用户昵称")
    private String nickname;

    @Schema(description = "头像URL")
    private String avatar;

    @Schema(description = "邮箱")
    private String email;

    @Schema(description = "是否开启推送通知")
    private Boolean pushEnabled;

    @Schema(description = "短信是否已经推送")
    private Boolean smsEnabled;
}