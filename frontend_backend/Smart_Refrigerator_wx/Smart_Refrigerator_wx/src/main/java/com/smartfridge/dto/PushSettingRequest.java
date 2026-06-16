package com.smartfridge.dto;

import io.swagger.v3.oas.annotations.media.Schema;
import jakarta.validation.constraints.NotNull;
import lombok.Data;

/**
 * 推送设置请求
 */
@Data
@Schema(description = "推送设置请求")
public class PushSettingRequest {

    @NotNull(message = "enabled不能为空")
    @Schema(description = "是否开启推送", example = "true")
    private Boolean pushEnabled;
}
