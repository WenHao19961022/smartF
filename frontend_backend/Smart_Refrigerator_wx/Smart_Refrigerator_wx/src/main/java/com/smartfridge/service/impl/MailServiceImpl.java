package com.smartfridge.service.impl;

import com.smartfridge.dto.InventoryDTO;
import com.smartfridge.service.MailService;
import jakarta.mail.internet.MimeMessage;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.mail.javamail.JavaMailSender;
import org.springframework.mail.javamail.MimeMessageHelper;
import org.springframework.stereotype.Service;

import java.time.format.DateTimeFormatter;
import java.util.List;

/**
 * @Author:ZhangXinqi
 * @Description:
 * @Date: Created in 18:50 2026/4/30
 */
@Slf4j
@Service
public class MailServiceImpl implements MailService {

    @Autowired
    private JavaMailSender mailSender;

    @Value("${spring.mail.username}")
    private String from;

    @Override
    public void sendBatchFreshnessEmail(String to, List<InventoryDTO> fruitList) {
        if (fruitList == null || fruitList.isEmpty()) {
            return;
        }
        try {
            MimeMessage message = mailSender.createMimeMessage();
            MimeMessageHelper helper = new MimeMessageHelper(message, true, "UTF-8");

            helper.setFrom("智慧冰箱管家 <" + from + ">");
            helper.setTo(to);
            helper.setSubject("您的智能冰箱提醒您，以下食材需要关注");

            // 拼接水果行
            StringBuilder rows = new StringBuilder();
            DateTimeFormatter dtf = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm");
            for (InventoryDTO fruit : fruitList) {
                String stateColor = "#27ae60";
                if (fruit.getFreshness() <= 25) {
                    stateColor = "#ff4757";
                } else if (fruit.getFreshness() <= 50) {
                    stateColor = "#f39c12";
                }
                rows.append("<tr>")
                        .append("<td style='padding:10px 16px;border-bottom:1px solid #eee;'>").append(fruit.getFruitName()).append("</td>")
                        .append("<td style='padding:10px 16px;border-bottom:1px solid #eee;color:").append(stateColor).append(";font-weight:bold;'>").append(fruit.getFreshness()).append("%</td>")
                        .append("<td style='padding:10px 16px;border-bottom:1px solid #eee;'>").append(fruit.getEntryTime() != null ? fruit.getEntryTime().format(dtf) : "-").append("</td>")
                        .append("<td style='padding:10px 16px;border-bottom:1px solid #eee;'>").append(fruit.getExpireDesc() != null ? fruit.getExpireDesc() : "-").append("</td>")
                        .append("</tr>");
            }

            String htmlContent = "<html><body>" +
                    "<div style='border: 1px solid #eef; border-radius: 10px; padding: 20px; font-family: sans-serif;'>" +
                    "<h2 style='color: #27ae60;'>您的智能冰箱提醒您，以下食材需要关注</h2>" +
                    "<p style='color:#666;'>系统检测到以下食材新鲜度较低，请尽快处理：</p>" +
                    "<table style='width:100%;border-collapse:collapse;margin-top:15px;'>" +
                    "<thead>" +
                    "<tr style='background:#f4f6f3;'>" +
                    "<th style='padding:10px 16px;text-align:left;border-bottom:2px solid #ddd;'>食材名称</th>" +
                    "<th style='padding:10px 16px;text-align:left;border-bottom:2px solid #ddd;'>新鲜度</th>" +
                    "<th style='padding:10px 16px;text-align:left;border-bottom:2px solid #ddd;'>入库时间</th>" +
                    "<th style='padding:10px 16px;text-align:left;border-bottom:2px solid #ddd;'>临期说明</th>" +
                    "</tr>" +
                    "</thead>" +
                    "<tbody>" + rows + "</tbody>" +
                    "</table>" +
                    "<hr style='border:none;border-top:1px solid #eee;margin:20px 0;'>" +
                    "<footer style='font-size:12px;color:#999;'>此邮件由智慧冰箱监控系统自动发出，无需回复。</footer>" +
                    "</div>" +
                    "</body></html>";

            helper.setText(htmlContent, true);
            mailSender.send(message);
            log.info(">>> 批量预警邮件已成功发送至: {}, 食材数量: {}", to, fruitList.size());
        } catch (Exception e) {
            log.error(">>> 批量邮件发送异常, to={}", to, e);
        }
    }
}
