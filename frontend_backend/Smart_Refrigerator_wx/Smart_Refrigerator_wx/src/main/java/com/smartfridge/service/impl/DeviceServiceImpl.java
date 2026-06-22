package com.smartfridge.service.impl;

import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
import com.smartfridge.entity.DeviceStatus;
import com.smartfridge.entity.UserDevice;
import com.smartfridge.mapper.DeviceStatusMapper;
import com.smartfridge.mapper.UserDeviceMapper;
import com.smartfridge.service.DeviceService;
import com.smartfridge.vo.DeviceStatusVO;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;
import java.util.List;
import java.util.Objects;

/**
 * 设备管理服务实现类
 */
@Service
public class DeviceServiceImpl implements DeviceService {

    @Autowired
    private DeviceStatusMapper deviceStatusMapper;

    @Autowired
    private UserDeviceMapper userDeviceMapper;

    @Override
    public DeviceStatusVO getDeviceStatus(String deviceSn) {
        DeviceStatus status = deviceStatusMapper.getByDeviceSn(deviceSn);
        DeviceStatusVO statusVO = new DeviceStatusVO();
        if (status == null) {
            // 首次查询，创建默认记录
            // 注意：MyBatis-Plus insert() 不会回填本地对象的值，必须手动设置
            // 否则下方 getUpdateTime() 为 null，在线判定逻辑将被跳过，设备永远显示离线
            LocalDateTime now = LocalDateTime.now();
            status = new DeviceStatus();
            status.setDeviceSn(deviceSn);
            status.setOnline(0);
            status.setCreateTime(now);
            status.setUpdateTime(now);
            deviceStatusMapper.insert(status);
        }

        // 判断是否离线(超过5分钟无心跳),更新时间就是最后一次心跳的时间
        if (status.getUpdateTime() != null) {
            status.setOnline(status.getUpdateTime().isAfter(
                    LocalDateTime.now().minusMinutes(5)) ? 1 : 0);
        }
        statusVO.setDeviceSn(deviceSn);
        statusVO.setOnline(status.getOnline());
        statusVO.setHumidity(Objects.equals(status.getOnline(), 1) ? status.getHumidity() : null);
        statusVO.setTemperature(Objects.equals(status.getOnline(), 1) ? status.getTemperature() : null);
        statusVO.setVisionStatus(status.getVisionStatus());
        statusVO.setVisionErrorTime(status.getVisionErrorTime());
        return statusVO;
    }

    @Override
    public void updateVisionStatus(String deviceSn, Integer visionStatus) {
        DeviceStatus status = deviceStatusMapper.getByDeviceSn(deviceSn);
        if (status == null) {
            status = new DeviceStatus();
            status.setDeviceSn(deviceSn);
            status.setOnline(1);
            status.setVisionStatus(visionStatus);
            status.setVisionErrorTime(visionStatus != null && visionStatus != 0 ? LocalDateTime.now() : null);
            status.setCreateTime(LocalDateTime.now());
            status.setUpdateTime(LocalDateTime.now());
            deviceStatusMapper.insert(status);
            return;
        }
        status.setVisionStatus(visionStatus);
        if (visionStatus != null && visionStatus != 0) status.setVisionErrorTime(LocalDateTime.now());
        status.setUpdateTime(LocalDateTime.now());
        deviceStatusMapper.updateById(status);
    }

    @Override
    public void updateDeviceStatus(String deviceSn, Integer online, Float temperature, Float humidity, String messageId) {
        DeviceStatus status = deviceStatusMapper.getByDeviceSn(deviceSn);
        LocalDateTime now = LocalDateTime.now();

        if (status == null) {
            status = new DeviceStatus();
            status.setDeviceSn(deviceSn);
            status.setOnline(online);
            status.setTemperature(temperature);
            status.setHumidity(humidity);
            status.setBatchId(messageId);
            status.setCreateTime(now);
            status.setUpdateTime(now);
            deviceStatusMapper.insert(status);
        } else {
            // 复用已查出的 status 实体，手动设置 updateTime 后走 updateById()
            // 让 @TableField(fill = INSERT_UPDATE) 注解生效，且 Java/MySQL 时间来源统一
            status.setOnline(online);
            status.setTemperature(temperature);
            status.setHumidity(humidity);
            status.setBatchId(messageId);
            status.setUpdateTime(now);
            deviceStatusMapper.updateById(status);
        }
    }

    @Override
    public void bindDevice(String openid, String deviceSn) {
        // 用 selectCount + LambdaQueryWrapper 走联合索引，避免 selectList(null) 全表扫
        Long count = userDeviceMapper.selectCount(new LambdaQueryWrapper<UserDevice>()
                .eq(UserDevice::getOpenid, openid)
                .eq(UserDevice::getDeviceSn, deviceSn));

        if (count > 0) {
            return; // 已绑定
        }

        UserDevice userDevice = new UserDevice();
        userDevice.setOpenid(openid);
        userDevice.setDeviceSn(deviceSn);
        userDeviceMapper.insert(userDevice);
    }

    @Override
    public List<UserDevice> getUserDevices(String openid) {
        return userDeviceMapper.getDevicesByOpenid(openid);
    }

    @Override
    public List<UserDevice> getDeviceUsers(String deviceSn) {
        return userDeviceMapper.getUsersByDeviceSn(deviceSn);
    }
}
