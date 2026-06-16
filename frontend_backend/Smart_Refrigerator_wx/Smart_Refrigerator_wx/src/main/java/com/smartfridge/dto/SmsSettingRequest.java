package com.smartfridge.dto;

import io.swagger.v3.oas.annotations.media.Schema;
import jakarta.validation.constraints.NotNull;
import lombok.Data;

/**
 * 短信提醒设置请求
 */
@Data
@Schema(description = "短信提醒设置请求")
public class SmsSettingRequest {

    @NotNull(message = "enabled不能为空")
    @Schema(description = "是否开启短信提醒", example = "true")
    private Boolean enabled;
}