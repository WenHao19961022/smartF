package com.smartfridge.controller;

import com.smartfridge.common.Result;
import com.smartfridge.dto.FruitCategoryDTO;
import com.smartfridge.dto.InventoryDTO;
import com.smartfridge.dto.UpdateInventoryRequest;
import com.smartfridge.service.InventoryService;
import io.swagger.v3.oas.annotations.Operation;
import io.swagger.v3.oas.annotations.Parameter;
import io.swagger.v3.oas.annotations.tags.Tag;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.validation.Valid;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.bind.annotation.*;

import java.util.List;

/**
 * 库存管理 Controller
 */
@Slf4j
@RestController
@RequestMapping("/inventory")
@Tag(name = "库存管理", description = "水果库存查询、修正、删除")
public class InventoryController {

    @Autowired
    private InventoryService inventoryService;

    /**
     * 获取实时库存列表
     * 计算新鲜度百分比和过期时间描述
     */
    @GetMapping("/list")
    @Operation(summary = "获取库存列表", description = "返回当前冰箱内所有水果库存，按新鲜度升序排列")
    public Result<List<InventoryDTO>> getInventoryList(
            @Parameter(description = "设备SN码") @RequestParam String deviceSn,
            @Parameter(description = "水果编码(可选)") @RequestParam(required = false) String fruitCode,
            HttpServletRequest request) {

        String openid = (String) request.getAttribute("openid");
        log.info("获取库存列表: openid={}, deviceSn={}", openid, deviceSn);

        List<InventoryDTO> list = inventoryService.getInventoryList(openid, deviceSn, fruitCode);
        return Result.success(list);
    }

    /**
     * 查询最新批次下所有水果类别及数量
     */
    @GetMapping("/fruitStatistic")
    @Operation(summary = "查询水果类别详情", description = "返回最新批次下所有水果类别及其数量和明细列表")
    public Result<List<FruitCategoryDTO>> getFruitDetail(
            @Parameter(description = "设备SN码") @RequestParam String deviceSn,
            HttpServletRequest request) {

        String openid = (String) request.getAttribute("openid");
        log.info("查询水果详情: openid={}, deviceSn={}", openid, deviceSn);

        List<FruitCategoryDTO> list = inventoryService.getFruitDetailByBatch(openid, deviceSn);
        return Result.success(list);
    }

    //最开始的一版本是不支持用户修改的，所以这个接口先留在这，可以方便后面用
//    /**
//     * 用户手动修正识别结果
//     * 关键逻辑：
//     * 1. 更新 custom_name 和 state
//     * 2. 设置 is_manual = 1（硬件不再覆盖）
//     * 3. 如果 state 从完整改为切开，重新计算过期时间
//     */
//    @PutMapping("/correct")
//    @Operation(summary = "修正识别结果", description = "用户手动修正水果名称或状态，设置后硬件数据不会覆盖")
//    public Result<Void> correctInventory(
//            @Valid @RequestBody UpdateInventoryRequest request,
//            HttpServletRequest httpRequest) {
//
//        Long userId = (Long) httpRequest.getAttribute("userId");
//        log.info("修正库存: userId={}, inventoryId={}, fruitName={}, state={}",
//                userId, request.getInventoryId(), request.getFruitName(), request.getState());
//
//        inventoryService.updateInventory(
//                request.getInventoryId(),
//                request.getFruitName(),
//                request.getState(),
//                request.getWeight()
//        );
//        return Result.success();
//    }
//
//    /**
//     * 删除/标记库存（吃完了）
//     */
//    @DeleteMapping("/{id}")
//    @Operation(summary = "移除库存", description = "将库存标记为已删除（吃完了）")
//    public Result<Void> removeInventory(
//            @Parameter(description = "库存ID") @PathVariable Long id,
//            HttpServletRequest request) {
//
//        Long userId = (Long) request.getAttribute("userId");
//        log.info("移除库存: userId={}, inventoryId={}", userId, id);
//
//        inventoryService.markAsRemoved(id);
//        return Result.success();
//    }
}
