package com.smartfridge.config;

import com.smartfridge.security.JwtAuthInterceptor;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.servlet.config.annotation.CorsRegistry;
import org.springframework.web.servlet.config.annotation.InterceptorRegistry;
import org.springframework.web.servlet.config.annotation.ResourceHandlerRegistry;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;

/**
 * Web MVC 配置类
 * 1. 注册 JWT 认证拦截器
 * 2. 配置跨域支持
 * 3. 配置静态资源映射 (针对 Swagger/Knife4j)
 */
@Configuration
public class WebMvcConfig implements WebMvcConfigurer {

    @Autowired
    private JwtAuthInterceptor jwtAuthInterceptor;

    /**
     * 不需要 Token 认证的路径列表
     */
    private static final String[] EXCLUDE_PATHS = {
            "/auth/login",           // 微信登录接口
            "/auth/login-debug",     // 如果你写了 Postman 调试后门
            "/error",                // SpringBoot 默认错误处理路径

            // --- Knife4j / Swagger 文档相关资源 (必须放行，否则文档看不了) ---
            "/doc.html",
            "/webjars/**",
            "/swagger-resources/**",
            "/v3/api-docs/**",
            "/favicon.ico",
            "/device/status"         // 设备状态公开查询接口
    };

    /**
     * 注册拦截器
     */
    @Override
    public void addInterceptors(InterceptorRegistry registry) {
        registry.addInterceptor(jwtAuthInterceptor)
                .addPathPatterns("/**")        // 默认拦截所有路径
                .excludePathPatterns(EXCLUDE_PATHS) // 放行白名单
                .order(1); // 设置执行顺序，多个拦截器时有用
    }

    /**
     * 配置跨域
     * 解决本地开发及 Postman 联调时可能遇到的跨域问题
     */
    @Override
    public void addCorsMappings(CorsRegistry registry) {
        registry.addMapping("/**")
                .allowedOriginPatterns("*") // 允许所有来源
                .allowedMethods("GET", "POST", "PUT", "DELETE", "OPTIONS")
                .allowedHeaders("*")
                .allowCredentials(true)
                .maxAge(3600);
    }

    /**
     * 静态资源映射
     * 确保 Knife4j 的 HTML 页面能被正确找到
     */
    @Override
    public void addResourceHandlers(ResourceHandlerRegistry registry) {
        registry.addResourceHandler("doc.html")
                .addResourceLocations("classpath:/META-INF/resources/");
        registry.addResourceHandler("/webjars/**")
                .addResourceLocations("classpath:/META-INF/resources/webjars/");
    }
}