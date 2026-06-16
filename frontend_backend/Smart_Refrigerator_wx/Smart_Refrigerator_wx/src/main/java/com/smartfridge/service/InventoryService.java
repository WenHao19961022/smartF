package com.smartfridge.service;

import com.baomidou.mybatisplus.extension.service.IService;
import com.smartfridge.dto.FruitCategoryDTO;
import com.smartfridge.dto.InventoryDTO;
import com.smartfridge.dto.MqttPayloadDTO;
import com.smartfridge.entity.FridgeInventory;

import java.util.List;

/**
 * 库存管理服务接口 (只增不减快照版 + 安全校验)
 */
public interface InventoryService extends IService<FridgeInventory> {

    /**
     * 获取当前最新库存列表 (增加安全校验)
     *
     * @param openid    当前登录用户的微信OpenID
     * @param deviceSn  准备查询的设备SN
     * @param fruitCode 水果编码(可选，为null时返回所有库存)
     */
    List<InventoryDTO> getInventoryList(String openid, String deviceSn, String fruitCode);

    /**
     * 获取当前最新库存列表（不过滤水果编码，返回全部）
     */
    default List<InventoryDTO> getInventoryList(String openid, String deviceSn) {
        return getInventoryList(openid, deviceSn, null);
    }

    /**
     * 查询最新批次下所有水果类别及其数量
     *
     * @param openid   当前登录用户的微信OpenID
     * @param deviceSn 设备SN
     */
    List<FruitCategoryDTO> getFruitDetailByBatch(String openid, String deviceSn);

    /**
     * 存储全量快照 (由 MQTT 处理类调用)
     * 此处不需要校验用户，因为硬件是受信任的发送端
     */
    void saveInventorySnapshot(MqttPayloadDTO data);
}
