package com.smartfridge.mapper;

import com.baomidou.mybatisplus.core.mapper.BaseMapper;
import com.smartfridge.entity.Notification;
import org.apache.ibatis.annotations.Param;

import java.util.List;

/**
 * 通知Mapper
 */
public interface NotificationMapper extends BaseMapper<Notification> {

    List<Notification> getUnreadList(@Param("openid") String openid);

    int markAllRead(@Param("openid") String openid);
}
