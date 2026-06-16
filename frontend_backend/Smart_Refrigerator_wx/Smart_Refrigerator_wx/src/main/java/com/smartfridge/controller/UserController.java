package com.smartfridge.controller;

import com.smartfridge.common.Result;
import com.smartfridge.dto.*;
import com.smartfridge.entity.SysUser;
import com.smartfridge.service.UserService;
import io.swagger.v3.oas.annotations.Operation;
import io.swagger.v3.oas.annotations.tags.Tag;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.BeanUtils;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.validation.annotation.Validated;
import org.springframework.web.bind.annotation.*;

/**
 * 用户 Controller - 用户信息与设置
 */
@Slf4j
@RestController
@RequestMapping("/user")
@Tag(name = "用户管理", description = "用户信息、推送设置")
public class UserController {

    @Autowired
    private UserService userService;

    /**
     * 获取用户信息
     */
    @GetMapping("/info")
    @Operation(summary = "获取用户信息", description = "查询当前用户的详细信息")
    public Result<UserInfoResponse> getUserInfo(@RequestAttribute("openid") String openid) {
        SysUser user = userService.getUserInfo(openid);
        UserInfoResponse response = new UserInfoResponse();
        BeanUtils.copyProperties(user, response);
        response.setPushEnabled(user.getPushEnabled());
        response.setSmsEnabled(user.getSmsEnabled());
        return Result.success(response);
    }

    /**
     * 更新推送设置
     */
    @PostMapping("/push-setting")
    @Operation(summary = "更新推送设置", description = "开启或关闭消息推送")
    public Result<Void> updatePushSetting(
            @RequestAttribute("openid") String openid,
            @Validated @RequestBody PushSettingRequest request) {
        log.info("更新推送设置: openid={}, pushEnabled={}", openid, request.getPushEnabled());
        userService.updatePushEnabled(openid, request.getPushEnabled());
        return Result.success();
    }

    /**
     * 更新短信提醒设置
     */
//    @PostMapping("/sms-setting")
//    @Operation(summary = "更新短信提醒设置", description = "开启或关闭短信提醒")
//    public Result<Void> updateSmsSetting(
//            @RequestAttribute("openid") String openid,
//            @Validated @RequestBody SmsSettingRequest request) {
//        log.info("更新短信提醒设置: openid={}, enabled={}", openid, request.getEnabled());
//        userService.updateSmsEnabled(openid, request.getEnabled());
//        return Result.success();
//    }

    /**
     * 更新邮箱
     */
    @PostMapping("/update-email")
    @Operation(summary = "更新邮箱", description = "绑定或更新邮箱")
    public Result<Void> updateEmail(
            @RequestAttribute("openid") String openid,
            @Validated @RequestBody UpdateEmailRequest request) {
        log.info("更新邮箱: openid={}, email={}", openid, request.getEmail());
        userService.updateEmail(openid, request.getEmail());
        return Result.success();
    }
}
