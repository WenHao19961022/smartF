-- =====================================================
-- 智能冰箱管理系统数据库初始化脚本
-- Database: smart_fridge
-- =====================================================

CREATE DATABASE IF NOT EXISTS smart_fridge DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE smart_fridge;

-- =====================================================
-- 1. 用户表 (sys_user)
-- =====================================================
CREATE TABLE IF NOT EXISTS sys_user (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '用户ID',
    openid VARCHAR(64) UNIQUE NOT NULL COMMENT '微信唯一标识',
    nickname VARCHAR(32) DEFAULT '微信用户' COMMENT '用户昵称',
    avatar VARCHAR(255) DEFAULT '' COMMENT '头像URL',
    push_enabled TINYINT(1) DEFAULT 1 COMMENT '是否开启推送通知',
    phone_number VARCHAR(20) DEFAULT '' COMMENT '手机号',
    sms_enabled TINYINT(1) DEFAULT 0 COMMENT '是否开启短信提醒',
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '注册时间'
) ENGINE=InnoDB COMMENT='系统用户表';

-- =====================================================
-- 2. 水果百科字典表 (fruit_dict)
-- =====================================================
CREATE TABLE IF NOT EXISTS fruit_dict (
    id INT PRIMARY KEY AUTO_INCREMENT COMMENT '主键',
    name VARCHAR(32) NOT NULL COMMENT '水果名称',
    category_code VARCHAR(32) UNIQUE COMMENT '硬件识别码(如:apple,banana)',
    shelf_life_whole INT DEFAULT 168 COMMENT '完整状态保质期(小时)',
    shelf_life_cut INT DEFAULT 24 COMMENT '切开状态保质期(小时)',
    nutrient JSON COMMENT '营养成分(JSON格式)',
    image_url VARCHAR(255) COMMENT '图片URL',
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    update_time DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB COMMENT='水果百科字典表';

-- 初始化水果字典数据
INSERT INTO fruit_dict (name, category_code, shelf_life_whole, shelf_life_cut, nutrient, image_url) VALUES
('红富士苹果', 'apple', 168, 24, '{"vitaminC": "4mg", "fiber": "2.4g", "calories": "52kcal"}', 'https://img.example.com/apple.png'),
('香蕉', 'banana', 120, 12, '{"vitaminC": "8.7mg", "fiber": "2.6g", "calories": "89kcal"}', 'https://img.example.com/banana.png'),
('橙子', 'orange', 240, 48, '{"vitaminC": "53.2mg", "fiber": "2.4g", "calories": "47kcal"}', 'https://img.example.com/orange.png'),
('西瓜', 'watermelon', 72, 8, '{"vitaminC": "8.1mg", "fiber": "0.4g", "calories": "30kcal"}', 'https://img.example.com/watermelon.png'),
('草莓', 'strawberry', 48, 6, '{"vitaminC": "58.8mg", "fiber": "2g", "calories": "32kcal"}', 'https://img.example.com/strawberry.png'),
('葡萄', 'grape', 96, 12, '{"vitaminC": "3.2mg", "fiber": "0.9g", "calories": "69kcal"}', 'https://img.example.com/grape.png'),
('芒果', 'mango', 72, 12, '{"vitaminC": "36.4mg", "fiber": "1.6g", "calories": "60kcal"}', 'https://img.example.com/mango.png'),
('梨', 'pear', 168, 36, '{"vitaminC": "4.3mg", "fiber": "3.1g", "calories": "57kcal"}', 'https://img.example.com/pear.png'),
('袋装水果', 'fruitInBags', 336, NULL, NULL, 'https://img.example.com/bagged_fruit.png');

-- =====================================================
-- 3. 实时库存表 (fridge_inventory)
-- =====================================================
CREATE TABLE IF NOT EXISTS fridge_inventory (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '主键',
    batch_id VARCHAR(64) NOT NULL COMMENT '快照批次/消息ID',
    device_id VARCHAR(64) NOT NULL COMMENT '设备SN码',
    track_id INT NOT NULL COMMENT '硬件算法生成的水果追踪ID',
    fruit_code VARCHAR(20) NOT NULL COMMENT '硬件水果类型代码',
    fruit_name VARCHAR(32) COMMENT '水果名称(冗余字段)',
    fresh_code VARCHAR(32) COMMENT '硬件新鲜度原始值/标签',
    fresh_score INT COMMENT '视觉新鲜度百分制分数',
    weight FLOAT DEFAULT 0 COMMENT '当前重量(g)',
    coordinate_x FLOAT DEFAULT 0 COMMENT '视觉坐标X，0-255',
    coordinate_y FLOAT DEFAULT 0 COMMENT '视觉坐标Y，0-255',
    entry_time DATETIME COMMENT '存入时间',
    expire_time DATETIME COMMENT '预估过期时间',
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    update_time DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_batch (batch_id),
    INDEX idx_device_id (device_id),
    INDEX idx_track (device_id, track_id)
) ENGINE=InnoDB COMMENT='实时库存表';

-- =====================================================
-- 4. 重量变化流水表 (weight_log)
-- =====================================================
CREATE TABLE IF NOT EXISTS weight_log (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '流水ID',
    inventory_id BIGINT COMMENT '关联库存ID',
    device_id VARCHAR(64) NOT NULL COMMENT '设备SN码',
    old_weight FLOAT COMMENT '旧重量(g)',
    new_weight FLOAT COMMENT '新重量(g)',
    weight_diff FLOAT COMMENT '重量变化(g)',
    msg_id VARCHAR(64) COMMENT '消息唯一ID(幂等)',
    log_time DATETIME COMMENT '记录时间',
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_device_time (device_id, log_time),
    UNIQUE KEY uk_msg_id (msg_id)
) ENGINE=InnoDB COMMENT='重量变化流水表';

-- =====================================================
-- 5. 用户设备关联表 (user_device)
-- =====================================================
CREATE TABLE IF NOT EXISTS user_device (
    id INT PRIMARY KEY AUTO_INCREMENT COMMENT '关联ID',
    openid VARCHAR(64) NOT NULL COMMENT '微信openid',
    device_sn VARCHAR(64) NOT NULL COMMENT '设备SN码',
    nickname VARCHAR(32) COMMENT '设备昵称',
    is_admin TINYINT(1) DEFAULT 0 COMMENT '是否管理员',
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_openid_device (openid, device_sn),
    INDEX idx_device_sn (device_sn)
) ENGINE=InnoDB COMMENT='用户设备关联表';

-- =====================================================
-- 6. 设备状态表 (device_status)
-- =====================================================
CREATE TABLE IF NOT EXISTS device_status (
    id INT PRIMARY KEY AUTO_INCREMENT COMMENT '状态ID',
    device_sn VARCHAR(64) UNIQUE NOT NULL COMMENT '设备SN码',
    online TINYINT(1) DEFAULT 0 COMMENT '0-离线, 1-在线',
    temperature FLOAT COMMENT '温度(℃)',
    humidity FLOAT COMMENT '湿度(%)',
    batch_id VARCHAR(64) COMMENT '最后一次库存快照批次ID',
    last_heartbeat DATETIME COMMENT '最后心跳时间',
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    update_time DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB COMMENT='设备状态表';

-- =====================================================
-- 7. 消息通知表 (notification)
-- =====================================================
CREATE TABLE IF NOT EXISTS notification (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '通知ID',
    openid VARCHAR(64) NOT NULL COMMENT '用户openid',
    device_sn VARCHAR(64) COMMENT '设备SN码',
    type VARCHAR(16) COMMENT '类型:warning,info,alert',
    title VARCHAR(64) COMMENT '标题',
    content VARCHAR(255) COMMENT '内容',
    is_read TINYINT(1) DEFAULT 0 COMMENT '0-未读, 1-已读',
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_openid_read (openid, is_read)
) ENGINE=InnoDB COMMENT='消息通知表';

-- =====================================================
-- 初始化设备数据（测试用）
-- =====================================================
INSERT INTO device_status (device_sn, online, temperature, humidity) VALUES
('FRIDGE-001', 1, 4.0, 65.0);

INSERT INTO user_device (openid, device_sn, nickname) VALUES
('test_openid_001', 'FRIDGE-001', '我的冰箱');

-- =====================================================
-- 完成
-- =====================================================
SELECT '数据库初始化完成!' AS message;
