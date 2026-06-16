package com.smartfridge.controller;

import com.smartfridge.common.Result;
import com.smartfridge.entity.DeviceStatus;
import com.smartfridge.entity.UserDevice;
import com.smartfridge.service.DeviceService;
import com.smartfridge.vo.DeviceStatusVO;
import io.swagger.v3.oas.annotations.Operation;
import io.swagger.v3.oas.annotations.Parameter;
import io.swagger.v3.oas.annotations.tags.Tag;
import jakarta.servlet.http.HttpServletRequest;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.bind.annotation.*;

import java.util.List;

/**
 * 设备管理 Controller
 */
@Slf4j
@RestController
@RequestMapping("/device")
@Tag(name = "设备管理", description = "设备状态查询、设备绑定、设备列表")
public class DeviceController {

    @Autowired
    private DeviceService deviceService;

    /**
     * 获取设备状态
     */
    @GetMapping("/status")
    @Operation(summary = "获取设备状态", description = "查询设备在线状态、温度、湿度等")
    public Result<DeviceStatusVO> getDeviceStatus(
            @Parameter(description = "设备SN码") @RequestParam String deviceSn) {

        DeviceStatusVO statusVO = deviceService.getDeviceStatus(deviceSn);
        return Result.success(statusVO);
    }

    /**
     * 绑定设备
     */
    @PostMapping("/bind")
    @Operation(summary = "绑定设备", description = "将设备绑定到当前用户")
    public Result<Void> bindDevice(
            @Parameter(description = "设备SN码") @RequestParam String deviceSn,
            HttpServletRequest request) {

        String openid = (String) request.getAttribute("openid");
        log.info("绑定设备: openid={}, deviceSn={}", openid, deviceSn);

        deviceService.bindDevice(openid, deviceSn);
        return Result.success();
    }

    /**
     * 获取用户绑定的设备列表
     */
    @GetMapping("/list")
    @Operation(summary = "设备列表", description = "获取当前用户绑定的所有设备")
    public Result<List<UserDevice>> getUserDevices(HttpServletRequest request) {
        String openid = (String) request.getAttribute("openid");
        log.info("获取设备列表: openid={}", openid);

        List<UserDevice> list = deviceService.getUserDevices(openid);
        return Result.success(list);
    }
}
