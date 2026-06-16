package com.smartfridge.dto;

import io.swagger.v3.oas.annotations.media.Schema;
import lombok.Builder;
import lombok.Data;

import java.util.List;

/**
 * 登录响应
 */
@Data
@Builder
@Schema(description = "登录响应")
public class LoginResponse {

    @Schema(description = "JWT Token")
    private String token;

    @Schema(description = "用户ID")
    private Long userId;

    @Schema(description = "用户昵称")
    private String nickname;

    @Schema(description = "用户头像")
    private String avatar;

    @Schema(description = "是否首次登录")
    private Boolean isNewUser;

    //设备号
    private List<String> deviceSnList;
}
