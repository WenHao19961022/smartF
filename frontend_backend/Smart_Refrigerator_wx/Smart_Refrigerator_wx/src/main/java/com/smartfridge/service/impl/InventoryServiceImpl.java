package com.smartfridge.service.impl;

import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
import com.baomidou.mybatisplus.core.conditions.query.QueryWrapper;
import com.baomidou.mybatisplus.extension.service.impl.ServiceImpl;
import com.smartfridge.common.BusinessException;
import com.smartfridge.dto.FruitCategoryDTO;
import com.smartfridge.dto.InventoryDTO;
import com.smartfridge.dto.MqttPayloadDTO;
import com.smartfridge.entity.DeviceStatus;
import com.smartfridge.entity.FridgeInventory;
import com.smartfridge.entity.FruitDict;
import com.smartfridge.entity.UserDevice;
import com.smartfridge.enums.FruitType;
import com.smartfridge.enums.FreshStatus;
import com.smartfridge.mapper.DeviceStatusMapper;
import com.smartfridge.mapper.FridgeInventoryMapper;
import com.smartfridge.mapper.FruitDictMapper;
import com.smartfridge.mapper.UserDeviceMapper;
import com.smartfridge.service.InventoryService;
import com.smartfridge.utils.FreshnessUtil;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.time.format.DateTimeParseException;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Function;
import java.util.stream.Collectors;

/**
 * 库存管理服务实现类 (只增不减快照版 + 安全校验)
 */
@Slf4j
@Service
public class InventoryServiceImpl extends ServiceImpl<FridgeInventoryMapper, FridgeInventory>
        implements InventoryService {

    @Autowired
    private FruitDictMapper fruitDictMapper;

    @Autowired
    private UserDeviceMapper userDeviceMapper;

    @Autowired
    private DeviceStatusMapper deviceStatusMapper;

    // ========== fruit_dict 缓存 ==========
    // 字典表变更不频繁，内存缓存 + 定期刷新，避免每次 MQTT 消息都全表扫
    private volatile Map<String, FruitDict> dictCache = new ConcurrentHashMap<>();
    private volatile long dictCacheRefreshTime = 0;
    private static final long DICT_CACHE_TTL_MS = 5 * 60 * 1000L;  // 5分钟
    // ====================================

    // ========== 袋装水果重量修正公式 ==========
    // 研究结论: 袋装水果的重量与保质期呈弱对数关系, 用对数+双界封顶实现"边际递减 + 范围保护"
    // shelfLifeHours = baseHours * max(0.7, min(1.5, 1.0 + 0.15 * log10(weight / 500)))
    // 参考: USDA FoodKeeper / 家庭冰箱袋装水果 2-8°C 保鲜期
    private static final double BAGGED_WEIGHT_REFERENCE_G = 500.0;   // 参考中位重量
    private static final double BAGGED_WEIGHT_LOG_COEFF    = 0.15;    // 对数修正系数
    private static final double BAGGED_WEIGHT_FACTOR_MIN   = 0.7;     // 修正下限 (小袋/挤压风险)
    private static final double BAGGED_WEIGHT_FACTOR_MAX   = 1.5;     // 修正上限 (大袋饱和)
    private static final DateTimeFormatter HARDWARE_TIME_FORMATTER = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");
    // ====================================

    @Override
    public List<InventoryDTO> getInventoryList(String openid, String deviceSn, String fruitCode) {
        // 1. 安全校验：检查该用户是否绑定了这台设备
        validateUserDeviceAccess(openid, deviceSn);
        // 2.获取设备信息，里面会包含最后一次批次号
        DeviceStatus byDeviceSn = deviceStatusMapper.getByDeviceSn(deviceSn);

        // 2. 获取该设备最后一次上报的快照记录
//        FridgeInventory latestOne = this.getOne(new LambdaQueryWrapper<FridgeInventory>()
//                .eq(FridgeInventory::getDeviceId, deviceSn)
//                .orderByDesc(FridgeInventory::getCreateTime)
//                .last("LIMIT 1"));

        if (byDeviceSn == null || byDeviceSn.getBatchId() == null) {
            return Collections.emptyList();
        }

        String latestBatchId = byDeviceSn.getBatchId();

        Map<String, FruitDict> dictMap = getDictMap();

        // 3. 查询该批次下的所有物品，如果传入了fruitCode则进行过滤
        LambdaQueryWrapper<FridgeInventory> wrapper = new LambdaQueryWrapper<FridgeInventory>()
                .eq(FridgeInventory::getBatchId, latestBatchId);
        if (fruitCode != null && !fruitCode.isEmpty()) {
            wrapper.eq(FridgeInventory::getFruitCode, fruitCode);
        }
        List<FridgeInventory> currentSnapshot = this.list(wrapper);

        // 4. 转换为前端需要的 DTO
        return currentSnapshot.stream().map(inv -> {
            // 通过 FruitType 枚举将 type 代码转为 categoryCode
            String fruitTypeCode = inv.getFruitCode();
            FruitType fruitType = resolveFruitType(fruitTypeCode);
            String categoryCode = fruitType != null ? fruitType.getCategoryCode() : fruitTypeCode;
            FruitDict dict = dictMap.get(categoryCode);
            Integer shelfLifeHours = (dict != null && dict.getShelfLifeWhole() != null) ? dict.getShelfLifeWhole() : (7 * 24);

            // 视觉新鲜度（存储在数据库的原始值）
            Integer visionScore = inv.getFreshScore();

            // 计算最终新鲜度（视觉值 × 时间衰减因子）
            Integer finalScore = FreshnessUtil.calcFinalScore(visionScore, inv.getExpireTime(), shelfLifeHours, LocalDateTime.now());

            // 预警判定：视觉腐烂 或 最终新鲜度≤30
            boolean isExpiring = FreshnessUtil.checkIsExpiring(finalScore, inv.getExpireTime(), shelfLifeHours, LocalDateTime.now());

            InventoryDTO dto = new InventoryDTO();
            dto.setInventoryId(inv.getTrackId());//这个是区分是苹果1还是苹果2的
            dto.setFruitName(inv.getFruitName());//水果名称
            dto.setFruitCode(inv.getFruitCode());//水果类型
            //dto.setQuantity(inv.getQuantity());
            dto.setEntryTime(inv.getEntryTime());//入库时间
            //这是临期时间的展示，入库的时候会算临期时间TODO目前是置为null
            dto.setExpireTime(inv.getExpireTime());
            dto.setShelfLifeWhole(shelfLifeHours);
            dto.setFreshness(finalScore);//新鲜度：使用最终综合新鲜度
            dto.setVisionScore(visionScore);//视觉新鲜度原始值
            dto.setFinalScore(finalScore);//最终新鲜度
            dto.setImageUrl(dict != null ? dict.getImageUrl() : null);//水果的URL
            dto.setIsExpiring(isExpiring);//是否需要预警
            dto.setWeight(inv.getWeight());
            dto.setCoordinateX(inv.getCoordinateX());
            dto.setCoordinateY(inv.getCoordinateY());
            return dto;
        }).collect(Collectors.toList());
    }

    /**
     * 安全校验逻辑：用户与设备绑定关系检查
     */
    private void validateUserDeviceAccess(String openid, String deviceSn) {
        // 在 user_device 表中查找匹配记录
        Long count = userDeviceMapper.selectCount(new LambdaQueryWrapper<UserDevice>()
                .eq(UserDevice::getOpenid, openid)
                .eq(UserDevice::getDeviceSn, deviceSn));

        if (count == 0) {
            log.warn("非法访问尝试: 用户 {} 试图访问未绑定的设备 {}", openid, deviceSn);
            // 抛出自定义异常，由全局异常处理器捕获返回给前端
            throw new BusinessException(403, "您无权访问该设备");
        }
    }

    @Override
    @Transactional(rollbackFor = Exception.class)
    public void saveInventorySnapshot(MqttPayloadDTO data) {
        String deviceId = data.getDeviceId();
        String batchId = data.getMessageId();
        LocalDateTime now = LocalDateTime.now();

        if (data.getFruitNum() == null || data.getFruitNum() == 0 || data.getFruits() == null || data.getFruits().isEmpty()) {
            return;
        }

        // 预加载字典（5分钟缓存，避免每次 MQTT 全表扫 fruit_dict）
        Map<String, FruitDict> dictMap = getDictMap();

        // 收集所有有效 trackId，一次批量查询首次记录（消除 N+1）
        Set<Integer> validTrackIds = data.getFruits().stream()
                .filter(item -> item.getId() != null && item.getType() != null && !item.getType().isBlank())
                .map(MqttPayloadDTO.FruitItemDTO::getId)
                .collect(Collectors.toSet());
        Map<Integer, FridgeInventory> firstRecordMap = batchFindFirstRecords(deviceId, validTrackIds);

        int dropCount = data.getFruits().size() - validTrackIds.size();
        if (dropCount > 0) {
            log.warn("MQTT消息中 {} 个水果缺少 trackId 已被丢弃: deviceId={}, msgId={}",
                    dropCount, deviceId, batchId);
        }

        List<FridgeInventory> entities = data.getFruits().stream()
                .filter(item -> item.getId() != null && item.getType() != null && !item.getType().isBlank())
                .map(item -> {
                    String fruitTypeCode = item.getType();
                    FruitType fruitType = resolveFruitType(fruitTypeCode);
                    String categoryCode = fruitType != null ? fruitType.getCategoryCode() : fruitTypeCode;
                    FruitDict dict = dictMap.get(categoryCode);
                    Float itemWeight = item.getWeight() != null ? item.getWeight() : 0F;

                    FridgeInventory inv = new FridgeInventory();
                    inv.setDeviceId(deviceId);
                    inv.setBatchId(batchId);
                    inv.setTrackId(item.getId());
                    inv.setFruitCode(fruitTypeCode);
                    inv.setFruitName(dict != null ? dict.getName() : (fruitType != null ? fruitType.getName() : "未知水果"));
                    inv.setFreshScore(FreshStatus.toPercent(item.getFreshStatus()));
                    inv.setFreshCode(item.getFreshStatus());
                    inv.setWeight(itemWeight);

                    // 核心逻辑：从批量查询的一次性记录中获取首次出现记录
                    FridgeInventory firstRecord = firstRecordMap.get(item.getId());
                    LocalDateTime entryTime = (firstRecord != null) ? firstRecord.getEntryTime() : parseHardwareTime(item.getPutInTime(), now);
                    inv.setEntryTime(entryTime);

                    long shelfLifeHours = (dict != null && dict.getShelfLifeWhole() != null) ? dict.getShelfLifeWhole() : (7 * 24);

                    // 袋装水果：按重量修正保质期（对数弱修正，研究结论表明重量与保鲜期非线性相关）
                    if (FruitType.FRUIT_IN_BAGS.getCategoryCode().equals(categoryCode)) {
                        double w = Math.max(itemWeight, 50.0);  // 防呆：最低按 50g 计算
                        double factor = 1.0 + BAGGED_WEIGHT_LOG_COEFF * Math.log10(w / BAGGED_WEIGHT_REFERENCE_G);
                        factor = Math.max(BAGGED_WEIGHT_FACTOR_MIN, Math.min(BAGGED_WEIGHT_FACTOR_MAX, factor));
                        shelfLifeHours = (long) (shelfLifeHours * factor);
                    }

                    inv.setExpireTime(firstRecord != null ? firstRecord.getExpireTime() : entryTime.plusHours(shelfLifeHours));

                    inv.setCoordinateX(item.getCoordinateX());
                    inv.setCoordinateY(item.getCoordinateY());

                    return inv;
                }).collect(Collectors.toList());

        if (entities.isEmpty()) {
            log.warn("MQTT消息没有可入库水果: deviceId={}, msgId={}", deviceId, batchId);
            return;
        }

        this.saveBatch(entities);
    }

    @Override
    public List<FruitCategoryDTO> getFruitDetailByBatch(String openid, String deviceSn) {
        // 1. 安全校验
        validateUserDeviceAccess(openid, deviceSn);

        // 2. 获取设备最新批次号
        DeviceStatus deviceStatus = deviceStatusMapper.getByDeviceSn(deviceSn);
        if (deviceStatus == null || deviceStatus.getBatchId() == null) {
            return Collections.emptyList();
        }

        // 3. 查询最新批次下所有记录
        List<FridgeInventory> allItems = this.list(new LambdaQueryWrapper<FridgeInventory>()
                .eq(FridgeInventory::getBatchId, deviceStatus.getBatchId()));

        // 4. 预加载字典（复用 5 分钟缓存）
        Map<String, FruitDict> dictMap = getDictMap();

        // 5. 按水果类别分组
        Map<String, List<FridgeInventory>> grouped = allItems.stream()
                .collect(Collectors.groupingBy(FridgeInventory::getFruitCode));

        // 6. 转换为 FruitCategoryDTO
        LocalDateTime now = LocalDateTime.now();
        return grouped.entrySet().stream().map(entry -> {
            String fruitCode = entry.getKey();
            List<FridgeInventory> items = entry.getValue();
            FruitType fruitType = resolveFruitType(fruitCode);
            FruitDict dict = dictMap.get(fruitType != null ? fruitType.getCategoryCode() : fruitCode);
            String fruitName = (dict != null) ? dict.getName() : items.get(0).getFruitName();
            Integer shelfLifeHours = (dict != null && dict.getShelfLifeWhole() != null) ? dict.getShelfLifeWhole() : (7 * 24);

            // 判断该类别是否有任何水果需要预警
            boolean hasWarning = items.stream().anyMatch(inv -> {
                Integer visionScore = inv.getFreshScore();
                Integer finalScore = FreshnessUtil.calcFinalScore(visionScore, inv.getExpireTime(), shelfLifeHours, now);
                return FreshnessUtil.checkIsExpiring(finalScore, inv.getExpireTime(), shelfLifeHours, now);
            });

            FruitCategoryDTO result = new FruitCategoryDTO();
            result.setFruitCode(fruitCode);
            result.setFruitName(fruitName);
            result.setCount(items.size());
            result.setImageUrl(dict != null ? dict.getImageUrl() : null);
            result.setWarningSignal(hasWarning);
            return result;
        }).collect(Collectors.toList());
    }

    /**
     * 辅助：批量查找多个 trackId 的首次出现记录（一次 IN 查询）
     * 解决 N+1 查询问题：原实现每个水果查一次 DB，现在一次查完所有水果
     * 返回 Map<trackId, firstRecord>，未找到的 trackId 不在 Map 中
     */
    private Map<Integer, FridgeInventory> batchFindFirstRecords(String deviceId, Set<Integer> trackIds) {
        if (trackIds == null || trackIds.isEmpty()) {
            return Collections.emptyMap();
        }
        List<FridgeInventory> all = this.list(new LambdaQueryWrapper<FridgeInventory>()
                .eq(FridgeInventory::getDeviceId, deviceId)
                .in(FridgeInventory::getTrackId, trackIds)
                .orderByAsc(FridgeInventory::getCreateTime));
        return all.stream()
                .collect(Collectors.toMap(
                        FridgeInventory::getTrackId,
                        Function.identity(),
                        (existing, replacement) -> existing  // 保留最早的（已按 createTime ASC 排序）
                ));
    }

    private FruitType resolveFruitType(String fruitTypeCode) {
        if (fruitTypeCode == null || fruitTypeCode.isBlank()) {
            return null;
        }
        try {
            return FruitType.getByCode(Integer.parseInt(fruitTypeCode));
        } catch (NumberFormatException ignored) {
            return FruitType.getByCategoryCode(fruitTypeCode);
        }
    }

    private LocalDateTime parseHardwareTime(String value, LocalDateTime fallback) {
        if (value == null || value.isBlank()) {
            return fallback;
        }
        try {
            return LocalDateTime.parse(value.trim(), HARDWARE_TIME_FORMATTER);
        } catch (DateTimeParseException e) {
            log.warn("硬件 put_in_time 格式异常，使用服务端时间兜底: {}", value);
            return fallback;
        }
    }

    /**
     * 获取 fruit_dict 全表 Map<categoryCode, FruitDict>，带 5 分钟内存缓存
     * 避免每次 MQTT 消息或前端查询都 SELECT * FROM fruit_dict
     */
    private Map<String, FruitDict> getDictMap() {
        long now = System.currentTimeMillis();
        if (dictCache.isEmpty() || now - dictCacheRefreshTime > DICT_CACHE_TTL_MS) {
            synchronized (this) {
                if (dictCache.isEmpty() || now - dictCacheRefreshTime > DICT_CACHE_TTL_MS) {
                    List<FruitDict> all = fruitDictMapper.selectList(new QueryWrapper<>());
                    Map<String, FruitDict> fresh = all.stream()
                            .collect(Collectors.toMap(
                                    FruitDict::getCategoryCode,
                                    Function.identity(),
                                    (a, b) -> a,
                                    ConcurrentHashMap::new));
                    dictCache = fresh;
                    dictCacheRefreshTime = now;
                    log.debug("fruit_dict 缓存刷新，加载 {} 条记录", all.size());
                }
            }
        }
        return dictCache;
    }
}
