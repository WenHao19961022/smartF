package com.smartfridge.security;

import io.jsonwebtoken.*;
import io.jsonwebtoken.security.Keys;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;

import javax.crypto.SecretKey;
import jakarta.annotation.PostConstruct;
import java.nio.charset.StandardCharsets;
import java.util.Date;

@Slf4j
@Component
public class JwtUtil {

    /** Token即将过期判定阈值：30分钟 */
    private static final long EXPIRING_SOON_THRESHOLD_MILLIS = 30 * 60 * 1000L;

    @Value("${jwt.secret:SmartFridgeJwtSecretKey2024VerySecure256Bits}")
    private String secretString;

    @Value("${jwt.expiration:86400000}")
    private long expiration;

    private SecretKey secretKey;

    /**
     * 线程本地缓存：避免同一请求内多次解析Token
     */
    private final ThreadLocal<String> tokenCache = new ThreadLocal<>();
    private final ThreadLocal<Claims> claimsCache = new ThreadLocal<>();

    @PostConstruct
    public void init() {
        this.secretKey = Keys.hmacShaKeyFor(secretString.getBytes(StandardCharsets.UTF_8));
    }

    /**
     * 生成Token
     */
    public String generateToken(Long userId, String openid) {
        Date now = new Date();
        String token = Jwts.builder()
                .subject(String.valueOf(userId))
                .claim("openid", openid)
                .issuedAt(now)
                .expiration(new Date(now.getTime() + expiration))
                .signWith(secretKey)
                .compact();
        log.debug("生成Token: userId={}", userId);
        return token;
    }

    /**
     * 解析Token（内部方法，自动缓存避免重复解析）
     */
    private Claims parseTokenInternal(String token) {
        String currentToken = tokenCache.get();
        Claims claims = claimsCache.get();

        if (token.equals(currentToken) && claims != null) {
            return claims;
        }

        try {
            claims = Jwts.parser()
                    .verifyWith(secretKey)
                    .build()
                    .parseSignedClaims(token)
                    .getPayload();
        } catch (ExpiredJwtException e) {
            log.warn("JWT已过期: {}", e.getMessage());
            throw e;
        } catch (JwtException e) {
            log.error("JWT解析失败: {}", e.getMessage());
            throw new RuntimeException("无效的Token");
        }

        tokenCache.set(token);
        claimsCache.set(claims);
        return claims;
    }

    /**
     * 清理线程本地缓存（每次请求结束时必须调用）
     */
    public void clearThreadCache() {
        tokenCache.remove();
        claimsCache.remove();
    }

    public Long getUserIdFromToken(String token) {
        return Long.parseLong(parseTokenInternal(token).getSubject());
    }

    public String getOpenidFromToken(String token) {
        return parseTokenInternal(token).get("openid", String.class);
    }

    /**
     * 验证Token是否有效
     */
    public boolean validateToken(String token) {
        try {
            parseTokenInternal(token);
            return true;
        } catch (JwtException e) {
            return false;
        }
    }

    /**
     * 判断Token是否即将过期（剩余有效期不足30分钟）
     */
    public boolean isTokenExpiringSoon(String token) {
        try {
            Claims claims = parseTokenInternal(token);
            Date expiration = claims.getExpiration();
            return expiration.getTime() - System.currentTimeMillis() < EXPIRING_SOON_THRESHOLD_MILLIS;
        } catch (ExpiredJwtException e) {
            return true;
        } catch (JwtException e) {
            return false;
        }
    }

    /**
     * 从Token中获取剩余有效期（毫秒），供外部监控使用
     */
    public long getRemainingValidity(String token) {
        try {
            Claims claims = parseTokenInternal(token);
            return Math.max(0, claims.getExpiration().getTime() - System.currentTimeMillis());
        } catch (ExpiredJwtException e) {
            return 0;
        } catch (JwtException e) {
            return -1;
        }
    }
}