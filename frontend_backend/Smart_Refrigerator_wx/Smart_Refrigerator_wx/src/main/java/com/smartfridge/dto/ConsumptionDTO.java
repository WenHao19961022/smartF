package com.smartfridge.dto;

import lombok.Data;
import java.util.List;

/**
 * 消耗曲线DTO
 */
@Data
public class ConsumptionDTO {
    private List<String> dates;
    private List<Float> weights;
}