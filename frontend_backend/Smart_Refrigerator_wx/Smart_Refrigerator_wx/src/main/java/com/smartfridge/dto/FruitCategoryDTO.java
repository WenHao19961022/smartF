package com.smartfridge.dto;

import lombok.Data;

import java.util.List;

/**
 * 按水果类别分组的详情DTO
 */
@Data
public class FruitCategoryDTO {

    /**
     * 水果编码（类别代码）
     */
    private String fruitCode;

    /**
     * 水果名称
     */
    private String fruitName;

    /**
     * 该类别下的水果数量
     */
    private Integer count;

    /**
     * 该类别下的水果详情列表
     */
    //private List<InventoryDTO> items;

    private Boolean warningSignal;

    private String imageUrl;
}
