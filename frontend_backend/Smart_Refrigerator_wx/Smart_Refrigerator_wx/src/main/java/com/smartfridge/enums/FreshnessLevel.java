package com.smartfridge.enums;

import lombok.AllArgsConstructor;
import lombok.Getter;

/**
 * 水果新鲜度等级枚举
 */
@Getter
@AllArgsConstructor
public enum FreshnessLevel {

    EXCELLENT(1, 100, "极新鲜", "水果状态极佳"),
    VERY_GOOD(2, 90, "很新鲜", "水果状态很好"),
    GOOD(3, 80, "新鲜", "水果状态良好"),
    FAIR(4, 70, "较新鲜", "建议尽快食用"),
    MODERATE(5, 60, "一般", "请优先食用"),
    LOW(6, 50, "不太新鲜", "请尽快食用"),
    POOR(7, 40, "不新鲜", "建议立即食用"),
    WARNING(8, 30, "临近过期", "即将过期，请立即食用"),
    CRITICAL(9, 20, "即将过期", "新鲜度很低，请立即处理"),
    EXPIRED(10, 0, "已过期", "水果已过期，不建议食用");

    private final Integer level;
    private final Integer freshnessPercent;
    private final String description;
    private final String suggestion;

    /**
     * 根据新鲜度百分比获取对应等级
     */
    public static FreshnessLevel getByFreshness(int freshnessPercent) {
        if (freshnessPercent >= 100) return EXCELLENT;
        if (freshnessPercent >= 90) return VERY_GOOD;
        if (freshnessPercent >= 80) return GOOD;
        if (freshnessPercent >= 70) return FAIR;
        if (freshnessPercent >= 60) return MODERATE;
        if (freshnessPercent >= 50) return LOW;
        if (freshnessPercent >= 40) return POOR;
        if (freshnessPercent >= 30) return WARNING;
        if (freshnessPercent >= 20) return CRITICAL;
        return EXPIRED;
    }

    /**
     * 是否需要预警
     */
    public boolean needAlert() {
        return this.level >= WARNING.level;
    }

    /**
     * 根据level获取百分比数值
     */
    public static int getPercentByLevel(int level) {
        return switch (level) {
            case 1 -> 100;
            case 2 -> 90;
            case 3 -> 80;
            case 4 -> 70;
            case 5 -> 60;
            case 6 -> 50;
            case 7 -> 40;
            case 8 -> 30;
            case 9 -> 20;
            case 10 -> 0;
            default -> 0;
        };
    }
}