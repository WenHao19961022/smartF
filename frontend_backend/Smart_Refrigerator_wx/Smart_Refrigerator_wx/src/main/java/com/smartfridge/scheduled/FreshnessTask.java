package com.smartfridge.scheduled;

import cn.hutool.core.util.StrUtil;
import com.smartfridge.dto.InventoryDTO;
import com.smartfridge.entity.SysUser;
import com.smartfridge.entity.UserDevice;
import com.smartfridge.mapper.SysUserMapper;
import com.smartfridge.mapper.UserDeviceMapper;
import com.smartfridge.service.InventoryService;
import com.smartfridge.service.MailService;
import com.smartfridge.utils.AESUtil;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

import java.time.LocalDateTime;
import java.util.ArrayList;
import java.util.List;

/**
 * @Author:ZhangXinqi
 * @Description: 临期食材每日邮件预警定时任务
 * @Date: Created in 18:47 2026/4/30
 */
@Slf4j
@Component
public class FreshnessTask {

    @Autowired
    private SysUserMapper sysUserMapper;

    @Autowired
    private UserDeviceMapper userDeviceMapper;

    @Autowired
    private InventoryService inventoryService;

    @Autowired
    private MailService mailService;

    @Autowired
    private AESUtil aesUtil;

    @Scheduled(fixedRate = 10000)
    //@Scheduled(cron = "0 0 9 * * ?") // 每天上午 9 点执行
    public void checkAndEmail() {
        // 查询所有开启了推送且绑定了邮箱的用户
        List<SysUser> users = sysUserMapper.getSendEmailUserList();
        // 获取当前时间，作为计算预警的基准
        LocalDateTime now = LocalDateTime.now();

        for (SysUser user : users) {
            if (StrUtil.isBlank(user.getEmail())) {
                continue;
            }

            // 解密邮箱
            String email;
            try {
                email = aesUtil.decrypt(user.getEmail());
                if (StrUtil.isBlank(email)) {
                    continue;
                }
            } catch (Exception e) {
                log.error("邮箱解密失败: openid={}", user.getOpenid(), e);
                continue;
            }

            // 查询该用户关联的所有设备
            List<UserDevice> devices = userDeviceMapper.getDevicesByOpenid(user.getOpenid());
            if (devices == null || devices.isEmpty()) {
                continue;
            }

            // 收集该用户所有设备中需要预警的食材
            List<InventoryDTO> alertList = new ArrayList<>();
            for (UserDevice device : devices) {
                List<InventoryDTO> items = inventoryService.getInventoryList(user.getOpenid(), device.getDeviceSn());
                if (items == null || items.isEmpty()) {
                    continue;
                }

                for (InventoryDTO item : items) {
                    if (Boolean.TRUE.equals(item.getIsExpiring())) {
                        alertList.add(item);
                    }
                }
            }

            if (!alertList.isEmpty()) {
                mailService.sendBatchFreshnessEmail(email, alertList);
                log.info("批量预警邮件已发送: openid={}, email={}, 食材数量={}", user.getOpenid(), email, alertList.size());
            }
        }
    }
}