package com.smartfridge.dto;

import io.swagger.v3.oas.annotations.media.Schema;
import jakarta.validation.constraints.NotBlank;
import lombok.Data;

/**
 * 更新邮箱请求
 */
@Data
@Schema(description = "更新邮箱请求")
public class UpdateEmailRequest {

    @NotBlank(message = "邮箱不能为空")
    @Schema(description = "邮箱", example = "user@example.com")
    private String email;
}