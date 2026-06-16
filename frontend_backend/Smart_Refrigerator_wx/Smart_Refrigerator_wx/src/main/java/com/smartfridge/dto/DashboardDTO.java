package com.smartfridge.dto;

import io.swagger.v3.oas.annotations.media.Schema;
import lombok.Builder;
import lombok.Data;

import java.util.List;

/**
 * 首页仪表盘统计 DTO
 */
@Data
@Builder
@Schema(description = "首页仪表盘统计")
public class DashboardDTO {

    @Schema(description = "今日消耗量(g)")
    private Float todayConsumption;

    @Schema(description = "临期物品数量")
    private Integer expiringCount;

    @Schema(description = "临期物品列表")
    private List<ExpiringItem> expiringItems;

    @Schema(description = "健康建议")
    private String healthSuggestion;

    @Schema(description = "当前库存数量")
    private Integer totalCount;

    @Data
    @Builder
    @Schema(description = "临期物品")
    public static class ExpiringItem {
        @Schema(description = "库存ID")
        private Long inventoryId;

        @Schema(description = "水果名称")
        private String name;

        @Schema(description = "新鲜度百分比")
        private Integer freshness;

        @Schema(description = "过期描述")
        private String expireDesc;
    }
}
