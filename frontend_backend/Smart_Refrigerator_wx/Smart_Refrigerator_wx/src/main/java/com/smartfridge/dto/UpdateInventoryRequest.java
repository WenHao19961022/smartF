package com.smartfridge.dto;

import io.swagger.v3.oas.annotations.media.Schema;
import jakarta.validation.constraints.NotNull;
import lombok.Data;

/**
 * 修正库存请求
 */
@Data
@Schema(description = "修正库存请求")
public class UpdateInventoryRequest {

    @NotNull(message = "inventoryId不能为空")
    @Schema(description = "库存ID", requiredMode = Schema.RequiredMode.REQUIRED)
    private Long inventoryId;

    @Schema(description = "修正后的水果名称")
    private String fruitName;

    @Schema(description = "修正后的状态(0-完整, 1-切开)")
    private Integer state;

    @Schema(description = "修正后的重量(g)")
    private Float weight;
}
