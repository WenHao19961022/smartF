package com.smartfridge.utils;

import cn.hutool.core.util.StrUtil;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;

import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.Base64;

/**
 * AES-256-GCM 加密工具类
 * 用于手机号等敏感数据的加密存储
 */
@Slf4j
@Component
public class AESUtil {

    private static final String ALGORITHM = "AES/GCM/NoPadding";
    private static final int GCM_TAG_LENGTH = 128; // bits
    private static final int GCM_IV_LENGTH = 12;  // bytes

    @Value("${crypto.phone-key}")
    private String phoneKey;

    /**
     * 加密手机号
     * IV 随机生成，拼接在密文前返回
     *
     * @param plainText 明文手机号
     * @return Base64(IV + ciphertext)，存入数据库
     */
    public String encrypt(String plainText) {
        if (StrUtil.isBlank(plainText)) {
            return plainText;
        }
        try {
            byte[] iv = new byte[GCM_IV_LENGTH];
            new SecureRandom().nextBytes(iv);

            SecretKeySpec keySpec = new SecretKeySpec(
                    padKey(phoneKey).getBytes(StandardCharsets.UTF_8), "AES");
            GCMParameterSpec gcmSpec = new GCMParameterSpec(GCM_TAG_LENGTH, iv);

            Cipher cipher = Cipher.getInstance(ALGORITHM);
            cipher.init(Cipher.ENCRYPT_MODE, keySpec, gcmSpec);
            byte[] cipherText = cipher.doFinal(plainText.getBytes(StandardCharsets.UTF_8));

            // 拼接 IV + ciphertext
            byte[] combined = new byte[iv.length + cipherText.length];
            System.arraycopy(iv, 0, combined, 0, iv.length);
            System.arraycopy(cipherText, 0, combined, iv.length, cipherText.length);

            return Base64.getEncoder().encodeToString(combined);
        } catch (Exception e) {
            log.error("手机号加密失败: {}", plainText, e);
            throw new RuntimeException("手机号加密失败", e);
        }
    }

    /**
     * 解密手机号
     *
     * @param encryptedText Base64(IV + ciphertext)
     * @return 明文手机号
     */
    public String decrypt(String encryptedText) {
        if (StrUtil.isBlank(encryptedText)) {
            return encryptedText;
        }
        try {
            byte[] combined = Base64.getDecoder().decode(encryptedText);

            // 拆分 IV 和 ciphertext
            byte[] iv = new byte[GCM_IV_LENGTH];
            byte[] cipherText = new byte[combined.length - GCM_IV_LENGTH];
            System.arraycopy(combined, 0, iv, 0, GCM_IV_LENGTH);
            System.arraycopy(combined, GCM_IV_LENGTH, cipherText, 0, cipherText.length);

            SecretKeySpec keySpec = new SecretKeySpec(
                    padKey(phoneKey).getBytes(StandardCharsets.UTF_8), "AES");
            GCMParameterSpec gcmSpec = new GCMParameterSpec(GCM_TAG_LENGTH, iv);

            Cipher cipher = Cipher.getInstance(ALGORITHM);
            cipher.init(Cipher.DECRYPT_MODE, keySpec, gcmSpec);
            byte[] plainText = cipher.doFinal(cipherText);

            return new String(plainText, StandardCharsets.UTF_8);
        } catch (Exception e) {
            log.error("手机号解密失败: {}", encryptedText, e);
            throw new RuntimeException("手机号解密失败", e);
        }
    }

    /**
     * 将密钥补齐/截断至 32 字节（256bit）
     * 不足填 0，超长截断
     */
    private String padKey(String key) {
        byte[] keyBytes = key.getBytes(StandardCharsets.UTF_8);
        byte[] result = new byte[32];
        System.arraycopy(keyBytes, 0, result, 0,
                Math.min(keyBytes.length, 32));
        return new String(result, StandardCharsets.UTF_8);
    }
}
