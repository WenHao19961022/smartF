//package com.smartfridge.scheduled;
//
//import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
//import com.smartfridge.entity.DeviceStatus;
//import com.smartfridge.entity.FridgeInventory;
//import com.smartfridge.entity.FruitDict;
//import com.smartfridge.entity.UserDevice;
//import com.smartfridge.mapper.*;
//import com.smartfridge.service.FreshnessService;
//import com.smartfridge.service.NotificationService;
//import lombok.extern.slf4j.Slf4j;
//import org.springframework.beans.factory.annotation.Autowired;
//import org.springframework.beans.factory.annotation.Value;
//import org.springframework.scheduling.annotation.Scheduled;
//import org.springframework.stereotype.Component;
//
//import java.time.LocalDateTime;
//import java.util.List;
//import java.util.Map;
//import java.util.stream.Collectors;
//
///**
// * 定时任务
// * 1. 每小时扫描新鲜度
// * 2. 每天生成健康报告
// * 3. 检测设备离线
// */
//@Slf4j
//@Component
//public class FreshnessCheckTask {
//
//    @Autowired
//    private FridgeInventoryMapper inventoryMapper;
//
//    @Autowired
//    private FruitDictMapper fruitDictMapper;
//
//    @Autowired
//    private UserDeviceMapper userDeviceMapper;
//
//    @Autowired
//    private DeviceStatusMapper deviceStatusMapper;
//
//    @Autowired
//    private FreshnessService freshnessService;
//
//    @Autowired
//    private NotificationService notificationService;
//
//    /**
//     * 每小时执行：扫描所有在库水果的新鲜度
//     * 发现临期水果，给用户发送通知
//     */
//    @Scheduled(fixedRate = 3600000) // 每小时
//    public void checkFreshness() {
//        log.info("=== 开始执行新鲜度检查定时任务 ===");
//
//        // 查找所有在线设备
//        List<DeviceStatus> onlineDevices = deviceStatusMapper.selectList(
//                new LambdaQueryWrapper<DeviceStatus>().eq(DeviceStatus::getOnline, 1));
//        if (onlineDevices == null || onlineDevices.isEmpty()) {
//            return;
//        }
//
//        // 预加载水果字典
//        List<FruitDict> fruitDicts = fruitDictMapper.selectList(null);
//        Map<Integer, FruitDict> fruitMap = fruitDicts.stream()
//                .collect(Collectors.toMap(FruitDict::getId, f -> f));
//
//        for (DeviceStatus device : onlineDevices) {
//            try {
//                checkDeviceFreshness(device.getDeviceSn(), fruitMap);
//            } catch (Exception e) {
//                log.error("设备 {} 新鲜度检查失败", device.getDeviceSn(), e);
//            }
//        }
//
//        log.info("=== 新鲜度检查定时任务完成 ===");
//    }
//
//    /**
//     * 检查单个设备的水果新鲜度
//     */
//    private void checkDeviceFreshness(String deviceSn, Map<Integer, FruitDict> fruitMap) {
//        List<FridgeInventory> list = inventoryMapper.getActiveByDeviceId(deviceSn);
//        if (list == null || list.isEmpty()) {
//            return;
//        }
//
//        // 获取该设备绑定的所有用户
//        List<UserDevice> users = userDeviceMapper.getUsersByDeviceSn(deviceSn);
//        if (users == null || users.isEmpty()) {
//            return;
//        }
//
//        for (FridgeInventory inventory : list) {
//            FruitDict fruit = fruitMap.get(inventory.getFruitId());
//            if (fruit == null) continue;
//
//            int shelfLife = inventory.getState() == 0 ? fruit.getShelfLifeWhole() : fruit.getShelfLifeCut();
//            int freshness = freshnessService.calculateFreshness(inventory, shelfLife);
//
//            // 新鲜度低于阈值，发送通知
//            if (freshnessService.needReminder(freshness)) {
//                String fruitName = inventory.getFruitName();
//                String expireDesc = freshnessService.getExpireDescription(inventory, shelfLife);
//
//                String quantity = inventory.getCurrentWeight() != null
//                        ? String.format("%.1fkg", inventory.getCurrentWeight()) : "未知";
//
//                for (UserDevice user : users) {
//                    if (freshness <= 0) {
//                        // 已过期
//                        notificationService.createNotification(
//                                user.getOpenid(), deviceSn, "warning",
//                                "水果已过期",
//                                "您的" + fruitName + "已过期" + expireDesc + "，建议及时处理",
//                                true, fruitName, expireDesc, quantity);
//                    } else {
//                        // 即将过期
//                        notificationService.createNotification(
//                                user.getOpenid(), deviceSn, "info",
//                                "水果即将过期",
//                                "您的" + fruitName + expireDesc + "，当前新鲜度" + freshness + "%，建议优先食用",
//                                true, fruitName, expireDesc, quantity);
//                    }
//                }
//                log.info("已为设备 {} 的 {} 发送新鲜度提醒, 新鲜度: {}%", deviceSn, fruitName, freshness);
//            }
//        }
//    }
//
//    /**
//     * 每5分钟执行：检查设备是否离线
//     */
//    @Scheduled(fixedRate = 300000) // 5分钟
//    public void checkDeviceOffline() {
//        List<DeviceStatus> devices = deviceStatusMapper.selectList(null);
//        for (DeviceStatus device : devices) {
//            if (device.getLastHeartbeat() != null &&
//                    device.getLastHeartbeat().isBefore(LocalDateTime.now().minusMinutes(5))) {
//                // 设备离线
//                if (device.getOnline() == 1) {
//                    deviceStatusMapper.updateStatus(device.getDeviceSn(), 0, device.getTemperature(), device.getHumidity());
//
//                    // 通知用户
//                    List<UserDevice> users = userDeviceMapper.getUsersByDeviceSn(device.getDeviceSn());
//                    for (UserDevice user : users) {
//                        notificationService.createNotification(
//                                user.getOpenid(), device.getDeviceSn(), "alert",
//                                "设备离线通知",
//                                "您的智能冰箱(" + device.getDeviceSn() + ")已离线，请检查设备连接",
//                                false, null, null, null);
//                    }
//                    log.warn("设备 {} 已离线", device.getDeviceSn());
//                }
//            }
//        }
//    }
//}
