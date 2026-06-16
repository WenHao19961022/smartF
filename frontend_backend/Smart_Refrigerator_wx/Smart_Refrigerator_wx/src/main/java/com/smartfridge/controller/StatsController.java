//package com.smartfridge.controller;
//
//import com.smartfridge.common.Result;
//import com.smartfridge.dto.ConsumptionDTO;
//import com.smartfridge.dto.DashboardDTO;
//import com.smartfridge.dto.NutritionDTO;
//import com.smartfridge.service.StatsService;
//import io.swagger.v3.oas.annotations.Operation;
//import io.swagger.v3.oas.annotations.Parameter;
//import io.swagger.v3.oas.annotations.tags.Tag;
//import jakarta.servlet.http.HttpServletRequest;
//import lombok.extern.slf4j.Slf4j;
//import org.springframework.beans.factory.annotation.Autowired;
//import org.springframework.web.bind.annotation.*;
//
///**
// * 统计分析 Controller
// */
//@Slf4j
//@RestController
//@RequestMapping("/stats")
//@Tag(name = "统计分析", description = "消耗曲线、营养分析、首页统计")
//public class StatsController {
//
//    @Autowired
//    private StatsService statsService;
//
//    /**
//     * 首页仪表盘统计
//     */
//    @GetMapping("/dashboard")
//    @Operation(summary = "首页统计", description = "返回今日消耗量、临期物品数量、健康建议")
//    public Result<DashboardDTO> getDashboard(
//            @Parameter(description = "设备SN码") @RequestParam String deviceSn,
//            HttpServletRequest request) {
//
//        Long userId = (Long) request.getAttribute("userId");
//        log.info("获取首页统计: userId={}, deviceSn={}", userId, deviceSn);
//
//        DashboardDTO dashboard = statsService.getDashboardStats(deviceSn);
//        return Result.success(dashboard);
//    }
//
//    /**
//     * 获取消耗曲线
//     */
//    @GetMapping("/consumption")
//    @Operation(summary = "消耗曲线", description = "获取近N天的水果消耗量数据")
//    public Result<ConsumptionDTO> getConsumptionCurve(
//            @Parameter(description = "设备SN码") @RequestParam String deviceSn,
//            @Parameter(description = "天数，默认7天") @RequestParam(defaultValue = "7") int days,
//            HttpServletRequest request) {
//
//        ConsumptionDTO curve = statsService.getConsumptionCurve(deviceSn, days);
//        return Result.success(curve);
//    }
//
//    /**
//     * 获取营养分析报告
//     */
//    @GetMapping("/nutrition")
//    @Operation(summary = "营养分析", description = "获取近N天的营养摄入分析报告")
//    public Result<NutritionDTO> getNutritionReport(
//            @Parameter(description = "设备SN码") @RequestParam String deviceSn,
//            @Parameter(description = "天数，默认7天") @RequestParam(defaultValue = "7") int days,
//            HttpServletRequest request) {
//
//        NutritionDTO report = statsService.getNutritionReport(deviceSn, days);
//        return Result.success(report);
//    }
//}
