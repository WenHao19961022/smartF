package com.smartfridge.controller;

import cn.hutool.core.util.StrUtil;
import com.smartfridge.common.Result;
import com.smartfridge.dto.LoginResponse;
import com.smartfridge.dto.WxLoginRequest;
import com.smartfridge.entity.SysUser;
import com.smartfridge.entity.UserDevice;
import com.smartfridge.security.JwtUtil;
import com.smartfridge.service.DeviceService;
import com.smartfridge.service.RedisService;
import com.smartfridge.service.UserService;
import com.smartfridge.utils.WeChatUtil;
import io.swagger.v3.oas.annotations.Operation;
import io.swagger.v3.oas.annotations.tags.Tag;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.validation.annotation.Validated;
import org.springframework.web.bind.annotation.*;

import java.util.List;

/**
 * 认证 Controller - 处理微信登录和Token管理
 */
@Slf4j
@RestController
@RequestMapping("/auth")
@Tag(name = "认证接口", description = "微信登录、JWT Token管理")
public class AuthController {

    @Autowired
    private WeChatUtil weChatUtil;

    @Autowired
    private UserService userService;

    @Autowired
    private JwtUtil jwtUtil;

    @Autowired
    private RedisService redisService;

    @Autowired
    private DeviceService deviceService;

    /**
     * 微信登录
     * 流程：前端调用wx.login()获取code -> 后端用code换openid -> 创建/查找用户 -> 生成JWT -> 存储Redis -> 返回Token
     */
    @PostMapping("/login")
    @Operation(summary = "微信登录", description = "通过微信code换取JWT Token")
    public Result<LoginResponse> login(@Validated @RequestBody WxLoginRequest request) {
        log.info("收到微信登录请求: code={}", request.getCode());
        String openid;

        // --- 增加调试后门，这个是为了用postman调试用的 ---
        if ("test".equals(request.getCode())) {
            // 如果 code 是 test，直接给一个固定的测试用 openid，不去求微信服务器
            openid = "o6zAJszqQ5RMW0yxpkJyDN60a7Zg";
        } else {
            // 正常的生产逻辑
            openid = weChatUtil.getOpenidByCode(request.getCode());
        }
        // --- 后门结束 ---

        // 1. 用code换取openid
        //String openid = weChatUtil.getOpenidByCode(request.getCode());
        if (StrUtil.isBlank(openid)) {
            log.warn("微信登录失败: 无法获取openid, code={}", request.getCode());
            return Result.error(401, "微信登录失败，请检查AppID和Secret配置");
        }

        // 2. 判断是否为首次登录
        SysUser existUser = userService.findByOpenid(openid);
        boolean isNewUser = (existUser == null);

        // 3. 获取或创建用户
        SysUser user = userService.getOrCreateUser(openid, request.getNickname(), request.getAvatarUrl());

        // 4. 生成JWT Token
        String token = jwtUtil.generateToken(user.getId(), openid);

        // 5. 存储Token到Redis（用于后续验证和登出）
        redisService.storeToken(user.getId(), token);

        log.info("用户登录成功: userId={}, openid={}, isNewUser={}", user.getId(), openid, isNewUser);

        // 6. 获取用户绑定的设备列表
        List<UserDevice> userDevices = deviceService.getUserDevices(openid);
        List<String> deviceSnList = userDevices.stream()
                .map(UserDevice::getDeviceSn)
                .collect(java.util.stream.Collectors.toList());

        // 7. 返回登录响应
        return Result.success(LoginResponse.builder()
                .token(token)
                .userId(user.getId())
                .nickname(user.getNickname())
                .avatar(user.getAvatar())
                .isNewUser(isNewUser)
                .deviceSnList(deviceSnList)
                .build());
    }

    /**
     * 登出
     */
    @PostMapping("/logout")
    @Operation(summary = "用户登出", description = "清除Redis中的Token")
    public Result<Void> logout(@RequestAttribute(name = "userId") Long userId) {
        redisService.deleteToken(userId);
        log.info("用户登出: userId={}", userId);
        return Result.success();
    }

    /**
     * 刷新Token
     */
    @PostMapping("/refresh")
    @Operation(summary = "刷新Token", description = "获取新Token并延长有效期")
    public Result<LoginResponse> refreshToken(
            @RequestAttribute(name = "userId") Long userId,
            @RequestAttribute(name = "openid") String openid) {

        // 生成新Token
        String newToken = jwtUtil.generateToken(userId, openid);

        // 更新Redis
        redisService.storeToken(userId, newToken);

        log.info("Token刷新成功: userId={}", userId);

        return Result.success(LoginResponse.builder()
                .token(newToken)
                .userId(userId)
                .build());
    }
}