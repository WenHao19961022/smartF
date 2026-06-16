package com.smartfridge.security;

import cn.hutool.core.util.StrUtil;
import com.smartfridge.common.BusinessException;
import com.smartfridge.entity.SysUser;
import com.smartfridge.service.UserService;
import com.smartfridge.service.RedisService;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Component;
import org.springframework.web.servlet.HandlerInterceptor;

/**
 * JWT Token 认证拦截器 (SysUser 适配版)
 * 1. 校验 Token 合法性
 * 2. 【核心】校验 SysUser 表中用户是否存在 (解决删库后显示已登录的问题)
 * 3. 校验 Redis 状态
 */
@Slf4j
@Component
public class JwtAuthInterceptor implements HandlerInterceptor {

    private static final String AUTH_HEADER = "Authorization";
    private static final String TOKEN_PREFIX = "Bearer ";
    public static final String USER_ID_ATTR = "userId";
    public static final String OPENID_ATTR = "openid";

    @Autowired
    private JwtUtil jwtUtil;

    @Autowired
    private RedisService redisService;

    @Autowired
    private UserService userService; // 切换到你的 UserService

    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler) {
        // 1. 放行 OPTIONS 请求 (跨域预检)
        if ("OPTIONS".equalsIgnoreCase(request.getMethod())) {
            return true;
        }

        // 2. 获取并提取 Token
        String authHeader = request.getHeader(AUTH_HEADER);
        if (StrUtil.isBlank(authHeader) || !authHeader.startsWith(TOKEN_PREFIX)) {
            log.warn("未授权访问: 缺少Token, URI: {}", request.getRequestURI());
            throw new BusinessException(401, "请先登录");
        }

        String token = authHeader.substring(TOKEN_PREFIX.length());

        // 3. 验证 Token 签名与有效期 (JwtUtil 逻辑)
        if (!jwtUtil.validateToken(token)) {
            throw new BusinessException(401, "登录已过期，请重新登录");
        }

        // 4. 解析 Token
        String openid = jwtUtil.getOpenidFromToken(token);
        Long userId = jwtUtil.getUserIdFromToken(token);

        if (StrUtil.isBlank(openid)) {
            throw new BusinessException(401, "无效的身份凭证");
        }

        // 5. 【关键优化】：校验 SysUser 数据库记录
        // 解决你说的：数据库删了，但手机由于缓存 Token 依然显示登录的问题
        SysUser user = userService.findByOpenid(openid);

        if (user == null) {
            log.error("安全校验失败: 数据库中找不到用户 [{}], Token 判定为伪造或失效", openid);
            // 抛出 401 异常，配合前端 request.js 的自动清理逻辑
            throw new BusinessException(401, "账号信息不存在，请重新登录");
        }

        // 6. 校验 Redis (如果你的业务逻辑需要双重校验 Token 一致性)
        if (!redisService.validateTokenInRedis(userId, token)) {
            log.warn("Token冲突或已被强制退出: userId={}", userId);
            throw new BusinessException(401, "登录状态异常，请重新登录");
        }

        // 7. 刷新 Token 有效期 (滑动过期)
        if (jwtUtil.isTokenExpiringSoon(token)) {
            redisService.refreshToken(userId);
        }

        // 8. 存入请求属性，方便后续 Controller 使用
        request.setAttribute(USER_ID_ATTR, userId);
        request.setAttribute(OPENID_ATTR, openid);

        return true;
    }

    @Override
    public void afterCompletion(HttpServletRequest request, HttpServletResponse response, Object handler, Exception ex) {
        // 如果你的 JwtUtil 实现了线程缓存清理，这里可以保留
        try {
            jwtUtil.clearThreadCache();
        } catch (Exception ignored) {
        }
    }
}