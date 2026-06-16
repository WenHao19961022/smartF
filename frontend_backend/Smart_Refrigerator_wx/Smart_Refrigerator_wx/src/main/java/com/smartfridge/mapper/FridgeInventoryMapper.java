package com.smartfridge.mapper;

import com.baomidou.mybatisplus.core.mapper.BaseMapper;
import com.smartfridge.entity.FridgeInventory;
import org.apache.ibatis.annotations.Param;

import java.util.List;

/**
 * 冰箱库存Mapper
 */
public interface FridgeInventoryMapper extends BaseMapper<FridgeInventory> {

    List<FridgeInventory> getActiveByDeviceId(@Param("deviceId") String deviceId);

    FridgeInventory getById(@Param("id") Long id);

    FridgeInventory getByFruitAndState(@Param("deviceId") String deviceId, @Param("fruitId") Integer fruitId, @Param("state") Integer state);

    List<FridgeInventory> getNeedReminderList(@Param("deviceId") String deviceId);
}
