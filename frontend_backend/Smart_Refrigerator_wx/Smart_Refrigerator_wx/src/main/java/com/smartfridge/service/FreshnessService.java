//package com.smartfridge.service;
//
//import com.smartfridge.entity.FridgeInventory;
//import org.springframework.stereotype.Service;
//
//import java.time.LocalDateTime;
//import java.time.temporal.ChronoUnit;
//
///**
// * 新鲜度计算服务
// * 核心算法：根据水果种类、状态、存放时间动态计算新鲜度百分比
// */
//@Service
//public class FreshnessService {
//
//    /**
//     * 计算新鲜度百分比
//     * @param inventory 库存记录
//     * @param shelfLifeHours 水果保质期(小时)
//     * @return 新鲜度百分比 0-100
//     */
//    public int calculateFreshness(FridgeInventory inventory, int shelfLifeHours) {
//        LocalDateTime now = LocalDateTime.now();
//        LocalDateTime entryTime = inventory.getEntryTime();
//
//        // 已存放时间(小时)
//        long elapsedHours = ChronoUnit.HOURS.between(entryTime, now);
//        if (elapsedHours < 0) {
//            elapsedHours = 0;
//        }
//
//        // 计算新鲜度: (1 - 已用时间/总时间) * 100
//        double freshness = 1.0 - ((double) elapsedHours / shelfLifeHours);
//
//        // 限制在0-100范围内
//        freshness = Math.max(0, Math.min(1, freshness));
//
//        return (int) (freshness * 100);
//    }
//
//    /**
//     * 获取过期描述
//     */
//    public String getExpireDescription(FridgeInventory inventory, int shelfLifeHours) {
//        LocalDateTime now = LocalDateTime.now();
//        LocalDateTime expireTime = inventory.getExpireTime();
//
//        long remainingHours = ChronoUnit.HOURS.between(now, expireTime);
//
//        if (remainingHours < 0) {
//            return "已过期" + Math.abs(remainingHours) + "小时";
//        } else if (remainingHours < 1) {
//            long minutes = ChronoUnit.MINUTES.between(now, expireTime);
//            return "预计" + minutes + "分钟后过期";
//        } else if (remainingHours < 24) {
//            return "预计" + remainingHours + "小时后过期";
//        } else {
//            long days = remainingHours / 24;
//            return "预计" + days + "天后过期";
//        }
//    }
//
//    /**
//     * 判断是否需要提醒
//     * @param freshness 新鲜度百分比
//     * @return true-需要提醒
//     */
//    public boolean needReminder(int freshness) {
//        // 新鲜度低于20%时提醒
//        return freshness <= 20;
//    }
//
//    /**
//     * 判断是否已过期
//     */
//    public boolean isExpired(FridgeInventory inventory) {
//        return inventory.getExpireTime().isBefore(LocalDateTime.now());
//    }
//}
