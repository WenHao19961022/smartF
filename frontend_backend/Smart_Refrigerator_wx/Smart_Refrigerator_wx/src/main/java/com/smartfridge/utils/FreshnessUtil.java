package com.smartfridge.utils;

import java.time.Duration;
import java.time.LocalDateTime;

/**
 * 食材新鲜度与临期状态通用计算工具类
 */
public class FreshnessUtil {

    /**
     * 根据剩余时间比例计算时间衰减因子
     * @param expireTime     预期过期时间
     * @param shelfLifeHours 标准保质期（小时）
     * @param now            当前时间
     * @return 衰减因子 (0.0 ~ 1.0)
     */
    public static double getTimeDecayFactor(LocalDateTime expireTime, Integer shelfLifeHours, LocalDateTime now) {
        if (expireTime == null || shelfLifeHours == null || shelfLifeHours <= 0) {
            return 1.0; // 兜底：无法计算时不做衰减
        }

        // 已过期
        if (now.isAfter(expireTime)) {
            return 0.0;
        }

        // 剩余小时数
        long remainingHours = Duration.between(now, expireTime).toHours();
        if (remainingHours <= 0) return 0.0;

        // 剩余时间占总保质期的比例
        double remainingRatio = (double) remainingHours / shelfLifeHours;

        // 分段衰减
        if (remainingRatio > 0.6) {
            return 1.0;  // 时间充裕，不衰减
        } else if (remainingRatio > 0.3) {
            return 0.7;  // 进入注意期
        } else if (remainingRatio > 0) {
            return 0.4;  // 即将过期
        } else {
            return 0.0;  // 已过期
        }
    }

    /**
     * 计算最终新鲜度 = 视觉新鲜度 × 时间衰减因子
     */
    public static Integer calcFinalScore(Integer visionScore, LocalDateTime expireTime, Integer shelfLifeHours, LocalDateTime now) {
        if (visionScore == null) return null;
        double decayFactor = getTimeDecayFactor(expireTime, shelfLifeHours, now);
        return (int) Math.round(visionScore * decayFactor);
    }

    /**
     * 判断食材是否处于临期预警状态
     * @param freshScore     当前新鲜度评分 (0-100)
     * @param expireTime     绝对过期时间
     * @param shelfLifeHours 字典规定的标准保质期（小时）
     * @param now            当前比较基准时间
     * @return boolean       true代表需要预警，false代表安全
     */
    public static boolean checkIsExpiring(Integer freshScore, LocalDateTime expireTime, Integer shelfLifeHours, LocalDateTime now) {
        // 条件 1：视觉极度不新鲜（低于或等于25%），直接判死刑
        if (freshScore != null && freshScore <= 25) {
            return true;
        }

        // 条件 2：时间维度判定
        if (expireTime != null) {
            // 容错兜底：没传保质期就按默认 7 天（168小时）算
            long totalHours = (shelfLifeHours != null && shelfLifeHours > 0) ? shelfLifeHours : (7 * 24L);

            // 临界阈值：剩余时间不足总保质期的 30%
            long warningThreshold = (long) (totalHours * 0.3);

            // 当前时间 + 抢救期 是否超过了彻底死亡时间
            return now.plusHours(warningThreshold).isAfter(expireTime) || now.isEqual(expireTime);
        }

        return false;
    }
}