package com.smartfridge.service;

/**
 * @Author:ZhangXinqi
 * @Description:
 * @Date: Created in 18:42 2026/4/30
 */
import com.smartfridge.dto.InventoryDTO;

import java.util.List;

public interface MailService {

    /**
     * 批量发送新鲜度预警邮件（一次性发送多个食材）
     * @param to 收件人邮箱
     * @param fruitList 食材列表（水果名、新鲜度、入库时间、临期时间）
     */
    void sendBatchFreshnessEmail(String to, List<InventoryDTO> fruitList);
}
