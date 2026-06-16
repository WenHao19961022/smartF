package com.smartfridge.service.impl;

import com.smartfridge.service.RedisService;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;

import java.util.concurrent.TimeUnit;

/**
 * Redis 缓存服务实现类
 */
@Slf4j
@Service
public class RedisServiceImpl implements RedisService {

    /** Token存储前缀 */
    private static final String TOKEN_PREFIX = "smartfridge:token:";

    /** 库存缓存前缀 */
    private static final String INVENTORY_CACHE_PREFIX = "smartfridge:inventory:";

    /** 消息幂等性前缀 */
    private static final String MQTT_MSG_PREFIX = "smartfridge:mqtt:msg:";

    /** Token默认过期时间：24小时 */
    private static final long TOKEN_EXPIRE_SECONDS = 24 * 60 * 60;

    @Autowired
    private StringRedisTemplate redisTemplate;

    @Override
    public void storeToken(Long userId, String token) {
        String key = TOKEN_PREFIX + userId;
        redisTemplate.opsForValue().set(key, token, TOKEN_EXPIRE_SECONDS, TimeUnit.SECONDS);
        log.debug("存储用户Token: userId={}, key={}", userId, key);
    }

    @Override
    public String getToken(Long userId) {
        String key = TOKEN_PREFIX + userId;
        return redisTemplate.opsForValue().get(key);
    }

    @Override
    public void deleteToken(Long userId) {
        String key = TOKEN_PREFIX + userId;
        redisTemplate.delete(key);
        log.debug("删除用户Token: userId={}", userId);
    }

    @Override
    public boolean validateTokenInRedis(Long userId, String token) {
        String storedToken = getToken(userId);
        return storedToken != null && storedToken.equals(token);
    }

    @Override
    public void refreshToken(Long userId) {
        String key = TOKEN_PREFIX + userId;
        redisTemplate.expire(key, TOKEN_EXPIRE_SECONDS, TimeUnit.SECONDS);
    }

    @Override
    public boolean isMqttMessageProcessed(String deviceSn, String msgId) {
        String key = MQTT_MSG_PREFIX + deviceSn + ":" + msgId;
        // 使用 SETNX (setIfAbsent) 原子地"认领"消息：
        // - 首次调用：key 不存在，写入成功，返回 true（消息未处理，本线程应继续业务）
        // - 重复调用：key 已存在，写入失败，返回 false（重复消息，应直接丢弃）
        // 避免先 hasKey 再 set 的 TOCTOU 竞态。
        Boolean claimed = redisTemplate.opsForValue().setIfAbsent(key, "1", 1, TimeUnit.HOURS);
        return Boolean.TRUE.equals(claimed);
    }

    @Override
    public void releaseMqttMessageProcessed(String deviceSn, String msgId) {
        String key = MQTT_MSG_PREFIX + deviceSn + ":" + msgId;
        redisTemplate.delete(key);
        log.info("已释放MQTT去重标记: key={}", key);
    }

    @Override
    public void cacheInventory(String deviceSn, String inventoryJson) {
        String key = INVENTORY_CACHE_PREFIX + deviceSn;
        redisTemplate.opsForValue().set(key, inventoryJson, 5, TimeUnit.MINUTES);
    }

    @Override
    public String getInventoryCache(String deviceSn) {
        String key = INVENTORY_CACHE_PREFIX + deviceSn;
        return redisTemplate.opsForValue().get(key);
    }

    @Override
    public void deleteInventoryCache(String deviceSn) {
        String key = INVENTORY_CACHE_PREFIX + deviceSn;
        redisTemplate.delete(key);
    }
}
