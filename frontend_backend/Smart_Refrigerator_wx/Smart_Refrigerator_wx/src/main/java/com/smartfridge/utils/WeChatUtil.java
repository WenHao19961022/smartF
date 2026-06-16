package com.smartfridge.utils;

import cn.hutool.http.HttpUtil;
import cn.hutool.json.JSONObject;
import cn.hutool.json.JSONUtil;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;

/**
 * 微信工具类
 * 用于获取openid和发送订阅消息
 * 使用Hutool HTTP替代Apache HttpClient
 */
@Slf4j
@Component
public class WeChatUtil {

    @Value("${wechat.appid}")
    private String appId;

    @Value("${wechat.secret}")
    private String secret;

    @Value("${wechat.template-id}")
    private String templateId;

    /**
     * 通过code获取openid
     * @param code 微信登录凭证
     * @return openid，失败返回null
     */
    public String getOpenidByCode(String code) {
        String url = String.format(
                "https://api.weixin.qq.com/sns/jscode2session?appid=%s&secret=%s&js_code=%s&grant_type=authorization_code",
                appId, secret, code);

        try {
            String result = HttpUtil.get(url, 5000);
            JSONObject json = JSONUtil.parseObj(result);

            if (json.containsKey("errcode") && json.getInt("errcode") != 0) {
                log.error("获取openid失败: {}", result);
                return null;
            }
            return json.getStr("openid");
        } catch (Exception e) {
            log.error("获取openid异常", e);
            return null;
        }
    }

    /**
     * 获取access_token
     * @return access_token，失败返回null
     */
    public String getAccessToken() {
        String url = String.format(
                "https://api.weixin.qq.com/cgi-bin/token?grant_type=client_credential&appid=%s&secret=%s",
                appId, secret);

        try {
            String result = HttpUtil.get(url, 5000);
            JSONObject json = JSONUtil.parseObj(result);

            if (json.containsKey("access_token")) {
                return json.getStr("access_token");
            }
            log.error("获取access_token失败: {}", result);
            return null;
        } catch (Exception e) {
            log.error("获取access_token异常", e);
            return null;
        }
    }

    /**
     * 发送订阅消息
     * @param openid 用户openid
     * @param templateId 模板ID
     * @param data 消息数据
     * @return true-成功, false-失败
     */
    public boolean sendSubscribeMessage(String openid, String templateId, JSONObject data) {
        String accessToken = getAccessToken();
        if (accessToken == null) {
            return false;
        }

        String url = "https://api.weixin.qq.com/cgi-bin/message/subscribe/send?access_token=" + accessToken;

        JSONObject body = new JSONObject();
        body.set("touser", openid);
        body.set("template_id", templateId);
        body.set("data", data);

        try {
            String result = HttpUtil.post(url, body.toString(), 5000);
            JSONObject json = JSONUtil.parseObj(result);
            int errcode = json.getInt("errcode", -1);

            if (errcode != 0) {
                log.error("发送订阅消息失败: {}", result);
                return false;
            }
            return true;
        } catch (Exception e) {
            log.error("发送订阅消息异常", e);
            return false;
        }
    }

    /**
     * 发送过期预警订阅消息（使用默认模板）
     */
    public boolean sendExpiryNotice(String openid, String fruitName, String expireDesc) {
        JSONObject data = new JSONObject();
        data.set("thing1", new JSONObject().set("value", fruitName));
        data.set("thing2", new JSONObject().set("value", expireDesc));
        return sendSubscribeMessage(openid, templateId, data);
    }
}
