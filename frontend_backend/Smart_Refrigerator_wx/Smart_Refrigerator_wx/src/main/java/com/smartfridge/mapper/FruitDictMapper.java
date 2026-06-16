package com.smartfridge.mapper;

import com.baomidou.mybatisplus.core.mapper.BaseMapper;
import com.smartfridge.entity.FruitDict;
import org.apache.ibatis.annotations.Param;

/**
 * 水果字典Mapper
 */
public interface FruitDictMapper extends BaseMapper<FruitDict> {

    FruitDict getByCategoryCode(@Param("code") String code);
}
