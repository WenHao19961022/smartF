package com.smartfridge.mapper;

import com.baomidou.mybatisplus.core.mapper.BaseMapper;
import com.smartfridge.entity.DeviceStatus;
import org.apache.ibatis.annotations.Mapper;
import org.apache.ibatis.annotations.Param;

/**
 * 设备状态Mapper
 * 注意: 设备状态更新已统一走 MyBatis-Plus 的 updateById(), 不再使用自定义 updateStatus SQL,
 * 以保证 @TableField(fill = INSERT_UPDATE) 注解生效, 并统一时间来源(LocalDateTime.now())。
 */
@Mapper
public interface DeviceStatusMapper extends BaseMapper<DeviceStatus> {

    DeviceStatus getByDeviceSn(@Param("deviceSn") String deviceSn);
}
