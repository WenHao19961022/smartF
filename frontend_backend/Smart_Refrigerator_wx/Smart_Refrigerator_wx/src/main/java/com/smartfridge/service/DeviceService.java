package com.smartfridge.service;

import com.smartfridge.entity.UserDevice;
import com.smartfridge.vo.DeviceStatusVO;

import java.util.List;

/**
 * 设备管理服务接口
 */
public interface DeviceService {

    /**
     * 获取设备状态
     */
    DeviceStatusVO getDeviceStatus(String deviceSn);

    /**
     * 更新设备状态
     */
    void updateDeviceStatus(String deviceSn, Integer online, Float temperature, Float humidity, String messageId);

    void updateVisionStatus(String deviceSn, Integer visionStatus);

    /**
     * 绑定设备
     */
    void bindDevice(String openid, String deviceSn);

    /**
     * 获取用户绑定的设备列表
     */
    List<UserDevice> getUserDevices(String openid);

    /**
     * 获取设备绑定的用户列表
     */
    List<UserDevice> getDeviceUsers(String deviceSn);
}
