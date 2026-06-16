package com.smartfridge.service;

import com.smartfridge.entity.SysUser;

/**
 * 用户服务接口
 */
public interface UserService {

    /**
     * 根据openid查找用户
     * @param openid 微信openid
     * @return 用户对象，不存在返回null
     */
    SysUser findByOpenid(String openid);

    /**
     * 创建新用户
     * @param openid 微信openid
     * @param nickname 用户昵称
     * @param avatar 用户头像
     * @return 新建的用户对象
     */
    SysUser createUser(String openid, String nickname, String avatar);

    /**
     * 更新用户信息
     * @param openid 微信openid
     * @param nickname 用户昵称
     * @param avatar 用户头像
     */
    void updateUser(String openid, String nickname, String avatar);

    /**
     * 获取或创建用户
     * @param openid 微信openid
     * @param nickname 用户昵称
     * @param avatar 用户头像
     * @return 用户对象
     */
    SysUser getOrCreateUser(String openid, String nickname, String avatar);

    /**
     * 更新推送开关设置
     */
    void updatePushEnabled(String openid, boolean pushEnabled);

    /**
     * 更新邮箱
     */
    void updateEmail(String openid, String emailAddress);

    /**
     * 更新邮件提醒开关
     */
    void updateSmsEnabled(String openid, boolean enabled);

    /**
     * 获取用户信息
     */
    SysUser getUserInfo(String openid);
}
