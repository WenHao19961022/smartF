package com.smartfridge.enums;

import lombok.AllArgsConstructor;
import lombok.Getter;

/**
 * 硬件视觉新鲜度标签枚举
 * 将 fresh_xxx / rotten_xxx 标签映射为百分比
 */
@Getter
@AllArgsConstructor
public enum FreshStatus {

    FRESH("fresh", 90),
    ROTTEN("rotten", 15);

    private final String prefix;
    private final Integer percentage;

    /**
     * 根据标签字符串前缀匹配，返回对应百分比
     * 例如: "fresh_apple" → 90, "rotten_banana" → 15, 未知 → 50
     */
    public static Integer toPercent(String label) {
        if (label == null || label.isEmpty()) return 50;
        String normalized = label.trim().toLowerCase();
        try {
            int score = (int) Math.round(Double.parseDouble(normalized));
            return Math.max(0, Math.min(100, score));
        } catch (NumberFormatException ignored) {
        }
        for (FreshStatus status : values()) {
            if (normalized.startsWith(status.prefix)) {
                return status.percentage;
            }
        }
        return 50; // 兜底
    }

    /**
     * 判断标签是否为腐烂状态
     */
    public static boolean isRotten(String label) {
        return label != null && label.toLowerCase().startsWith(ROTTEN.prefix);
    }
}
