package com.smartfridge.service.impl;

import cn.hutool.core.util.StrUtil;
import com.smartfridge.common.BusinessException;
import com.smartfridge.entity.SysUser;
import com.smartfridge.mapper.SysUserMapper;
import com.smartfridge.service.UserService;
import com.smartfridge.utils.AESUtil;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;

/**
 * 用户服务实现类
 */
@Slf4j
@Service
public class UserServiceImpl implements UserService {

    @Autowired
    private SysUserMapper userMapper;

    @Autowired
    private AESUtil aesUtil;

    @Override
    public SysUser findByOpenid(String openid) {
        if (StrUtil.isBlank(openid)) {
            return null;
        }
        return userMapper.getByOpenid(openid);
    }

    @Override
    public SysUser createUser(String openid, String nickname, String avatar) {
        SysUser user = new SysUser();
        user.setOpenid(openid);
        user.setNickname(StrUtil.blankToDefault(nickname, "微信用户"));
        user.setAvatar(StrUtil.blankToDefault(avatar, ""));
        //user.setPushEnabled(0); // 默认关闭推送
        userMapper.insert(user);
        log.info("创建新用户: openid={}, nickname={}", openid, nickname);
        return user;
    }

    @Override
    public void updateUser(String openid, String nickname, String avatar) {
        SysUser user = findByOpenid(openid);
        if (user != null) {
            if (StrUtil.isNotBlank(nickname)) {
                user.setNickname(nickname);
            }
            if (StrUtil.isNotBlank(avatar)) {
                user.setAvatar(avatar);
            }
            userMapper.updateById(user);
        }
    }

    @Override
    public SysUser getOrCreateUser(String openid, String nickname, String avatar) {
        SysUser user = findByOpenid(openid);
        if (user == null) {
            user = createUser(openid, nickname, avatar);
        } else if (StrUtil.isNotBlank(nickname) || StrUtil.isNotBlank(avatar)) {
            // 已存在用户，更新信息
            updateUserInfo(user, nickname, avatar);
        }
        return user;
    }

    private void updateUserInfo(SysUser user, String nickname, String avatar) {
        boolean updated = false;
        if (StrUtil.isNotBlank(nickname) && !nickname.equals(user.getNickname())) {
            user.setNickname(nickname);
            updated = true;
        }
        if (StrUtil.isNotBlank(avatar) && !avatar.equals(user.getAvatar())) {
            user.setAvatar(avatar);
            updated = true;
        }
        if (updated) {
            userMapper.updateById(user);
            log.debug("更新用户信息: userId={}, nickname={}", user.getId(), nickname);
        }
    }

    @Override
    public void updatePushEnabled(String openid, boolean pushEnabled) {
        SysUser user = findByOpenid(openid);
        if (user == null) {
            throw new BusinessException(404, "用户不存在");
        }
        user.setPushEnabled(pushEnabled ? 1 : 0);
        user.setUpdateTime(LocalDateTime.now());
        userMapper.updateById(user);
        log.info("更新推送设置: openid={}, pushEnabled={}", openid, pushEnabled);
    }

    @Override
    public void updateEmail(String openid, String emailAddress) {
        SysUser user = findByOpenid(openid);
        if (user == null) {
            throw new BusinessException(404, "用户不存在");
        }
        // 加密后存储
        String encrypted = aesUtil.encrypt(emailAddress);
        user.setEmail(encrypted);
        user.setUpdateTime(LocalDateTime.now());
        userMapper.updateById(user);
        log.info("更新邮箱: openid={}, 已加密存储", openid);
    }

    @Override
    public void updateSmsEnabled(String openid, boolean enabled) {
        SysUser user = findByOpenid(openid);
        if (user == null) {
            throw new BusinessException(404, "用户不存在");
        }
        // 开启邮件提醒前校验邮箱（需解密后判断）
        String decryptedEmail = null;
        if (StrUtil.isNotBlank(user.getEmail())) {
            try {
                decryptedEmail = aesUtil.decrypt(user.getEmail());
            } catch (Exception e) {
                log.error("邮箱解密失败: openid={}", openid, e);
            }
        }
        if (enabled && StrUtil.isBlank(decryptedEmail)) {
            throw new BusinessException(400, "请先绑定邮箱");
        }
        user.setSmsEnabled(enabled ? 1 : 0);
        userMapper.updateById(user);
        log.info("更新邮件提醒设置: openid={}, smsEnabled={}", openid, enabled);
    }

    @Override
    public SysUser getUserInfo(String openid) {
        SysUser user = findByOpenid(openid);
        if (user == null) {
            throw new BusinessException(404, "用户不存在");
        }
        // 解密邮箱
        if (StrUtil.isNotBlank(user.getEmail())) {
            try {
                user.setEmail(aesUtil.decrypt(user.getEmail()));
            } catch (Exception e) {
                log.error("邮箱解密失败: openid={}", openid, e);
                user.setEmail(null);
            }
        }
        return user;
    }
}
