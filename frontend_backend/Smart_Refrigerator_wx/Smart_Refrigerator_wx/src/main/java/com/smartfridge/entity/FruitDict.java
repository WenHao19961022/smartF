package com.smartfridge.entity;

import com.baomidou.mybatisplus.annotation.*;
import lombok.Data;
import java.time.LocalDateTime;

/**
 * 水果百科字典实体
 */
@Data
@TableName("fruit_dict")
public class FruitDict {

    @TableId(type = IdType.AUTO)
    private Integer id;

    private String name;

    private String categoryCode;

    private Integer shelfLifeWhole;

    //private Integer shelfLifeCut;

    private String nutrient;

    private String imageUrl;

    @TableField(fill = FieldFill.INSERT)
    private LocalDateTime createTime;

//    @TableField(fill = FieldFill.INSERT_UPDATE)
//    private LocalDateTime updateTime;
}