package com.smartfridge.dto;

import lombok.Data;
import java.util.Map;

/**
 * 营养分析DTO
 */
@Data
public class NutritionDTO {
    /** 各营养素摄入总量 */
    private Map<String, String> nutrients;
    /** 健康建议 */
    private String suggestion;
    /** 总消耗重量(g) */
    private Float totalWeight;
}