//package com.smartfridge.service;
//
//import com.fasterxml.jackson.core.JsonProcessingException;
//import com.fasterxml.jackson.databind.JsonNode;
//import com.fasterxml.jackson.databind.ObjectMapper;
//import com.smartfridge.dto.ConsumptionDTO;
//import com.smartfridge.dto.DashboardDTO;
//import com.smartfridge.dto.NutritionDTO;
//import com.smartfridge.entity.FridgeInventory;
//import com.smartfridge.entity.FruitDict;
//import com.smartfridge.entity.InventoryLog;
//import com.smartfridge.mapper.FridgeInventoryMapper;
//import com.smartfridge.mapper.FruitDictMapper;
//import com.smartfridge.mapper.WeightLogMapper;
//import cn.hutool.core.util.StrUtil;
//import lombok.extern.slf4j.Slf4j;
//import org.springframework.beans.factory.annotation.Autowired;
//import org.springframework.stereotype.Service;
//
//import java.time.LocalDate;
//import java.time.LocalDateTime;
//import java.util.*;
//import java.util.stream.Collectors;
//
///**
// * 统计分析服务
// */
//@Slf4j
//@Service
//public class StatsService {
//
//    private static final ObjectMapper objectMapper = new ObjectMapper();
//
//    @Autowired
//    private WeightLogMapper weightLogMapper;
//
//    @Autowired
//    private FridgeInventoryMapper inventoryMapper;
//
//    @Autowired
//    private FruitDictMapper fruitDictMapper;
//
//    @Autowired
//    private FreshnessService freshnessService;
//
//    /**
//     * 获取首页仪表盘统计
//     */
//    public DashboardDTO getDashboardStats(String deviceSn) {
//        // 获取今日消耗量
//        LocalDateTime todayStart = LocalDate.now().atStartOfDay();
//        Float todayConsumption = weightLogMapper.getTotalConsumption(deviceSn, todayStart, LocalDateTime.now());
//        if (todayConsumption == null) todayConsumption = 0f;
//
//        // 获取当前库存
//        List<FridgeInventory> inventoryList = inventoryMapper.getActiveByDeviceId(deviceSn);
//
//        // 预加载水果字典
//        Map<Integer, FruitDict> fruitMap = fruitDictMapper.selectList(null).stream()
//                .collect(Collectors.toMap(FruitDict::getId, f -> f, (a, b) -> a));
//
//        // 临期物品
//        List<DashboardDTO.ExpiringItem> expiringItems = new ArrayList<>();
//        int expiringCount = 0;
//
//        for (FridgeInventory inv : inventoryList) {
//            FruitDict fruit = fruitMap.get(inv.getFruitId());
//            if (fruit == null) continue;
//
//            int shelfLife = inv.getState() == 0 ? fruit.getShelfLifeWhole() : fruit.getShelfLifeCut();
//            int freshness = freshnessService.calculateFreshness(inv, shelfLife);
//
//            if (freshness <= 20) {
//                expiringCount++;
//                expiringItems.add(DashboardDTO.ExpiringItem.builder()
//                        .inventoryId(inv.getId())
//                        .name(inv.getFruitName())
//                        .freshness(freshness)
//                        .expireDesc(freshnessService.getExpireDescription(inv, shelfLife))
//                        .build());
//            }
//        }
//
//        // 按新鲜度排序
//        expiringItems.sort(Comparator.comparingInt(DashboardDTO.ExpiringItem::getFreshness));
//
//        // 生成健康建议
//        String suggestion = generateHealthSuggestion(expiringCount, inventoryList.size());
//
//        return DashboardDTO.builder()
//                .todayConsumption(todayConsumption)
//                .expiringCount(expiringCount)
//                .expiringItems(expiringItems)
//                .healthSuggestion(suggestion)
//                .totalCount(inventoryList.size())
//                .build();
//    }
//
//    private String generateHealthSuggestion(int expiringCount, int totalCount) {
//        if (totalCount == 0) {
//            return "冰箱空空如也，记得补充新鲜水果哦！";
//        }
//        if (expiringCount > 3) {
//            return String.format("您有%d件水果即将过期，建议优先食用，同时注意不要一次购买太多。", expiringCount);
//        }
//        if (expiringCount > 0) {
//            return String.format("您有%d件水果新鲜度较低，建议近期优先食用。", expiringCount);
//        }
//        return "您的水果都很新鲜，继续保持健康的饮食习惯！";
//    }
//
//    /**
//     * 获取消耗曲线
//     */
//    public ConsumptionDTO getConsumptionCurve(String deviceSn, int days) {
//        LocalDateTime startTime = LocalDate.now().minusDays(days).atStartOfDay();
//
//        List<InventoryLog> logs = weightLogMapper.getLogsByDeviceAndTime(deviceSn, startTime);
//        if (logs == null || logs.isEmpty()) {
//            ConsumptionDTO dto = new ConsumptionDTO();
//            dto.setDates(Collections.emptyList());
//            dto.setWeights(Collections.emptyList());
//            return dto;
//        }
//
//        // 按日期分组汇总
//        Map<LocalDate, Float> dailyConsumption = new TreeMap<>();
//        for (InventoryLog log : logs) {
//            if (log.getWeightDiff() < 0) {
//                LocalDate date = log.getLogTime().toLocalDate();
//                dailyConsumption.merge(date, Math.abs(log.getWeightDiff()), Float::sum);
//            }
//        }
//
//        // 补全缺失日期
//        List<String> dates = new ArrayList<>();
//        List<Float> weights = new ArrayList<>();
//        for (int i = days - 1; i >= 0; i--) {
//            LocalDate date = LocalDate.now().minusDays(i);
//            dates.add(date.toString().substring(5)); // MM-dd格式
//            weights.add(dailyConsumption.getOrDefault(date, 0f));
//        }
//
//        ConsumptionDTO dto = new ConsumptionDTO();
//        dto.setDates(dates);
//        dto.setWeights(weights);
//        return dto;
//    }
//
//    /**
//     * 获取营养分析报告
//     */
//    public NutritionDTO getNutritionReport(String deviceSn, int days) {
//        LocalDateTime startTime = LocalDate.now().minusDays(days).atStartOfDay();
//
//        // 获取已消耗的水果(重量减少的记录)
//        List<InventoryLog> logs = weightLogMapper.getLogsByDeviceAndTime(deviceSn, startTime);
//        if (logs == null || logs.isEmpty()) {
//            NutritionDTO dto = new NutritionDTO();
//            dto.setNutrients(Collections.emptyMap());
//            dto.setSuggestion("暂无消耗记录");
//            dto.setTotalWeight(0f);
//            return dto;
//        }
//
//        // 加载水果字典
//        List<FruitDict> fruitDicts = fruitDictMapper.selectList(null);
//        Map<Integer, FruitDict> fruitMap = fruitDicts.stream()
//                .collect(Collectors.toMap(FruitDict::getId, f -> f));
//
//        // 统计每种水果消耗量
//        Map<Integer, Float> fruitConsumption = new HashMap<>();
//        float totalWeight = 0f;
//        for (InventoryLog log : logs) {
//            if (log.getWeightDiff() < 0 && log.getInventoryId() != null) {
//                FridgeInventory inv = inventoryMapper.selectById(log.getInventoryId());
//                if (inv != null) {
//                    fruitConsumption.merge(inv.getFruitId(), Math.abs(log.getWeightDiff()), Float::sum);
//                    totalWeight += Math.abs(log.getWeightDiff());
//                }
//            }
//        }
//
//        // 计算营养摄入
//        Map<String, String> nutrients = new LinkedHashMap<>();
//        Map<String, Float> vitaminCSum = new HashMap<>();
//        Map<String, Float> fiberSum = new HashMap<>();
//        Map<String, Float> caloriesSum = new HashMap<>();
//
//        for (Map.Entry<Integer, Float> entry : fruitConsumption.entrySet()) {
//            FruitDict fruit = fruitMap.get(entry.getKey());
//            if (fruit != null && StrUtil.isNotBlank(fruit.getNutrient())) {
//                JsonNode nutrientJson = null;
//                try {
//                    nutrientJson = objectMapper.readTree(fruit.getNutrient());
//                } catch (JsonProcessingException e) {
//                    throw new RuntimeException(e);
//                }
//                // 简化的营养计算: 按100g为单位
//                float portions = entry.getValue() / 100f;
//
//                String vitaminC = nutrientJson.get("vitaminC").asText();
//                String fiber = nutrientJson.get("fiber").asText();
//                String calories = nutrientJson.get("calories").asText();
//
//                // 累加营养素
//                accumulateNutrient(vitaminCSum, "vitaminC", vitaminC, portions);
//                accumulateNutrient(fiberSum, "fiber", fiber, portions);
//                accumulateNutrient(caloriesSum, "calories", calories, portions);
//            }
//        }
//
//        nutrients.put("维生素C", formatNutrient(vitaminCSum.get("vitaminC"), "mg"));
//        nutrients.put("膳食纤维", formatNutrient(fiberSum.get("fiber"), "g"));
//        nutrients.put("热量", formatNutrient(caloriesSum.get("calories"), "kcal"));
//
//        // 生成健康建议
//        String suggestion = generateSuggestion(vitaminCSum.get("vitaminC"), fiberSum.get("fiber"));
//
//        NutritionDTO dto = new NutritionDTO();
//        dto.setNutrients(nutrients);
//        dto.setSuggestion(suggestion);
//        dto.setTotalWeight(totalWeight);
//        return dto;
//    }
//
//    private void accumulateNutrient(Map<String, Float> map, String key, String value, float portions) {
//        if (value != null) {
//            float num = extractNumber(value);
//            map.merge(key, num * portions, Float::sum);
//        }
//    }
//
//    private float extractNumber(String str) {
//        if (str == null) return 0;
//        try {
//            return Float.parseFloat(str.replaceAll("[^0-9.]", ""));
//        } catch (NumberFormatException e) {
//            return 0;
//        }
//    }
//
//    private String formatNutrient(Float value, String unit) {
//        if (value == null || value == 0) return "0" + unit;
//        return String.format("%.1f%s", value, unit);
//    }
//
//    private String generateSuggestion(Float vitaminC, Float fiber) {
//        List<String> suggestions = new ArrayList<>();
//        float vc = vitaminC != null ? vitaminC : 0;
//        float fb = fiber != null ? fiber : 0;
//
//        if (vc < 50) {
//            suggestions.add("维生素C摄入不足，建议多补充柑橘类水果");
//        }
//        if (fb < 10) {
//            suggestions.add("膳食纤维摄入偏少，建议增加苹果、梨的摄入");
//        }
//        if (vc >= 50 && fb >= 10) {
//            suggestions.add("营养摄入均衡，继续保持");
//        }
//        return String.join("；", suggestions);
//    }
//}
