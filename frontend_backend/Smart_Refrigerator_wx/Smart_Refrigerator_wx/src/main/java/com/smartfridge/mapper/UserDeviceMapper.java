package com.smartfridge.mapper;

import com.baomidou.mybatisplus.core.mapper.BaseMapper;
import com.smartfridge.entity.UserDevice;
import org.apache.ibatis.annotations.Param;

import java.util.List;

/**
 * 用户设备关联Mapper
 */
public interface UserDeviceMapper extends BaseMapper<UserDevice> {

    List<UserDevice> getUsersByDeviceSn(@Param("deviceSn") String deviceSn);

    List<UserDevice> getDevicesByOpenid(@Param("openid") String openid);

    List<String> getDevicesCNListByOpenid(@Param("openid") String openid);
}
