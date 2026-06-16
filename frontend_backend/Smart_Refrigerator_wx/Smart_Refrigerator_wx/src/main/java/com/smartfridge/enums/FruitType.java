package com.smartfridge.enums;

import lombok.AllArgsConstructor;
import lombok.Getter;

/**
 * 水果类型枚举
 */
@Getter
@AllArgsConstructor
public enum FruitType {

    APPLE(1, "苹果", "apple"),
    BANANA(2, "香蕉", "banana"),
    ORANGE(3, "橙子", "orange"),
    GRAPE(4, "葡萄", "grape"),
    PEAR(5, "梨", "pear"),
    MANGO(6, "芒果", "mango"),
    FRUIT_IN_BAGS(7, "袋装水果", "fruitInBags");

    private final Integer code;
    private final String name;
    private final String categoryCode;

    /**
     * 根据code获取枚举
     */
    public static FruitType getByCode(Integer code) {
        if (code == null) return null;
        for (FruitType type : values()) {
            if (type.getCode().equals(code)) {
                return type;
            }
        }
        return null;
    }

    /**
     * 根据categoryCode获取枚举
     */
    public static FruitType getByCategoryCode(String categoryCode) {
        if (categoryCode == null) return null;
        for (FruitType type : values()) {
            if (type.getCategoryCode().equalsIgnoreCase(categoryCode)) {
                return type;
            }
        }
        return null;
    }
}
