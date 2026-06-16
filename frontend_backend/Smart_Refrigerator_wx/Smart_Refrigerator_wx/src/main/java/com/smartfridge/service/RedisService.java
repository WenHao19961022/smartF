package com.smartfridge.service;

/**
 * Redis 缓存服务接口
 * 用于存储Token、实时库存缓存等
 */
public interface RedisService {

    /**
     * 存储用户Token
     * @param userId 用户ID
     * @param token JWT Token
     */
    void storeToken(Long userId, String token);

    /**
     * 获取用户Token
     * @param userId 用户ID
     * @return Token字符串，不存在返回null
     */
    String getToken(Long userId);

    /**
     * 删除用户Token（用于登出）
     * @param userId 用户ID
     */
    void deleteToken(Long userId);

    /**
     * 验证Token是否与Redis中存储的一致
     * @param userId 用户ID
     * @param token JWT Token
     * @return true-一致, false-不一致或不存在
     */
    boolean validateTokenInRedis(Long userId, String token);

    /**
     * 刷新Token过期时间
     * @param userId 用户ID
     */
    void refreshToken(Long userId);

    /**
     * 原子地"认领"MQTT消息（基于 Redis SETNX）
     * 该调用同时完成检查与标记，由 Redis 服务端保证原子性，避免并发场景下
     * 多个消费者同时通过 check 后都执行处理的竞态问题。
     * Key 形如 smartfridge:mqtt:msg:{deviceSn}:{msgId}，按设备隔离避免跨设备误杀。
     *
     * @param deviceSn 设备序列号
     * @param msgId 消息唯一ID
     * @return true-消息未处理（本次成功认领，应继续处理业务）;
     *         false-消息已处理（重复消息，应直接丢弃）
     */
    boolean isMqttMessageProcessed(String deviceSn, String msgId);

    /**
     * 释放MQTT消息的去重标记。
     * 用于 DB 持久化失败后的回滚：让 MQTT 重传的消息能够被处理，避免永久丢消息。
     *
     * @param deviceSn 设备序列号
     * @param msgId 消息唯一ID
     */
    void releaseMqttMessageProcessed(String deviceSn, String msgId);

    /**
     * 缓存库存列表JSON
     * @param deviceSn 设备SN
     * @param inventoryJson 库存JSON字符串
     */
    void cacheInventory(String deviceSn, String inventoryJson);

    /**
     * 获取库存缓存
     * @param deviceSn 设备SN
     * @return 库存JSON，不存在返回null
     */
    String getInventoryCache(String deviceSn);

    /**
     * 删除库存缓存
     * @param deviceSn 设备SN
     */
    void deleteInventoryCache(String deviceSn);
}
