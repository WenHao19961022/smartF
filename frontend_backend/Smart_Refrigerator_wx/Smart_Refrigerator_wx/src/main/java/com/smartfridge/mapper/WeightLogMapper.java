package com.smartfridge.mapper;

import com.baomidou.mybatisplus.core.mapper.BaseMapper;
import com.smartfridge.entity.InventoryLog;
import org.apache.ibatis.annotations.Param;

import java.time.LocalDateTime;
import java.util.List;

/**
 * 重量变化流水Mapper
 */
public interface WeightLogMapper extends BaseMapper<InventoryLog> {

    Float getTotalConsumption(@Param("deviceId") String deviceId, @Param("startTime") LocalDateTime startTime, @Param("endTime") LocalDateTime endTime);

    List<InventoryLog> getLogsByDeviceAndTime(@Param("deviceId") String deviceId, @Param("startTime") LocalDateTime startTime);

    boolean existsByMsgId(@Param("msgId") String msgId);
}
