-- 创建数据库
CREATE DATABASE IF NOT EXISTS smart_fridge DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE smart_fridge;

-- 1. 水果百科字典表
CREATE TABLE fruit_dict (
    id INT PRIMARY KEY AUTO_INCREMENT COMMENT '主键',
    name VARCHAR(50) NOT NULL COMMENT '水果名称',
    category_code VARCHAR(50) NOT NULL COMMENT '视觉识别代码',
    shelf_life_whole INT NOT NULL DEFAULT 168 COMMENT '完整保质期(小时)',
    shelf_life_cut INT NOT NULL DEFAULT 24 COMMENT '切开保质期(小时)',
    nutrient TEXT COMMENT '营养信息JSON',
    image_url VARCHAR(255) COMMENT '图片URL',
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    update_time DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_code (category_code)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='水果百科字典表';

-- 2. 实时库存表
CREATE TABLE fridge_inventory (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '主键',
    batch_id VARCHAR(64) NOT NULL COMMENT '快照批次/消息ID',
    device_id VARCHAR(50) NOT NULL COMMENT '设备SN码',
    track_id INT NOT NULL COMMENT '硬件算法生成的水果追踪ID',
    fruit_code VARCHAR(20) NOT NULL COMMENT '硬件水果类型代码',
    fruit_name VARCHAR(50) NOT NULL COMMENT '水果名称(冗余字段便于查询)',
    fresh_code VARCHAR(32) COMMENT '硬件新鲜度原始值/标签',
    fresh_score INT COMMENT '视觉新鲜度百分制分数',
    weight FLOAT NOT NULL COMMENT '当前重量(g)',
    coordinate_x FLOAT DEFAULT 0 COMMENT '视觉坐标X，0-255',
    coordinate_y FLOAT DEFAULT 0 COMMENT '视觉坐标Y，0-255',
    entry_time DATETIME NOT NULL COMMENT '放入时间',
    expire_time DATETIME NOT NULL COMMENT '预估过期时间',
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    update_time DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_batch (batch_id),
    INDEX idx_device (device_id),
    INDEX idx_track (device_id, track_id),
    INDEX idx_expire (expire_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='实时库存表';

-- 3. 重量变化流水表
CREATE TABLE weight_log (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '主键',
    inventory_id BIGINT NOT NULL COMMENT '关联库存ID',
    device_id VARCHAR(50) NOT NULL COMMENT '设备SN码',
    old_weight FLOAT COMMENT '旧重量(g)',
    new_weight FLOAT COMMENT '新重量(g)',
    weight_diff FLOAT NOT NULL COMMENT '重量变化(g)',
    msg_id VARCHAR(100) COMMENT '消息唯一ID(幂等)',
    log_time DATETIME NOT NULL COMMENT '记录时间',
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_inventory (inventory_id),
    INDEX idx_device (device_id),
    INDEX idx_time (log_time),
    UNIQUE KEY uk_msg_id (msg_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='重量变化流水表';

-- 4. 用户设备关联表
CREATE TABLE user_device (
    id INT PRIMARY KEY AUTO_INCREMENT COMMENT '主键',
    openid VARCHAR(100) NOT NULL COMMENT '微信用户ID',
    device_sn VARCHAR(50) NOT NULL COMMENT '设备序列号',
    nickname VARCHAR(50) COMMENT '用户昵称',
    avatar_url VARCHAR(255) COMMENT '头像',
    is_admin TINYINT DEFAULT 0 COMMENT '是否管理员',
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_openid_device (openid, device_sn),
    INDEX idx_device (device_sn)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户设备关联表';

-- 5. 设备状态表
CREATE TABLE device_status (
    id INT PRIMARY KEY AUTO_INCREMENT COMMENT '主键',
    device_sn VARCHAR(50) NOT NULL UNIQUE COMMENT '设备序列号',
    online TINYINT DEFAULT 0 COMMENT '0-离线, 1-在线',
    temperature FLOAT COMMENT '温度(℃)',
    humidity FLOAT COMMENT '湿度(%)',
    batch_id VARCHAR(64) COMMENT '最后一次库存快照批次ID',
    vision_status INT DEFAULT 0 COMMENT '视觉健康状态，0=正常',
    vision_error_time DATETIME COMMENT '最后视觉错误时间',
    last_heartbeat DATETIME COMMENT '最后心跳时间',
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    update_time DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='设备状态表';

-- 6. 消息通知表
CREATE TABLE notification (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '主键',
    openid VARCHAR(100) NOT NULL COMMENT '用户ID',
    device_sn VARCHAR(50) NOT NULL COMMENT '设备SN',
    type VARCHAR(20) NOT NULL COMMENT '类型: warning, info, alert',
    title VARCHAR(100) NOT NULL COMMENT '标题',
    content VARCHAR(500) NOT NULL COMMENT '内容',
    is_read TINYINT DEFAULT 0 COMMENT '0-未读, 1-已读',
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_openid (openid),
    INDEX idx_device (device_sn)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='消息通知表';

-- 插入预设水果数据
INSERT INTO fruit_dict (name, category_code, shelf_life_whole, shelf_life_cut, nutrient, image_url) VALUES
('红富士苹果', 'apple', 168, 24, '{"vitaminC": "4mg/100g", "fiber": "2.4g/100g", "calories": "52kcal/100g"}', '/images/apple.png'),
('香蕉', 'banana', 120, 8, '{"vitaminC": "8.7mg/100g", "fiber": "2.6g/100g", "calories": "89kcal/100g"}', '/images/banana.png'),
('橙子', 'orange', 336, 48, '{"vitaminC": "53.2mg/100g", "fiber": "2.4g/100g", "calories": "47kcal/100g"}', '/images/orange.png'),
('西瓜', 'watermelon', 72, 12, '{"vitaminC": "8.1mg/100g", "fiber": "0.4g/100g", "calories": "30kcal/100g"}', '/images/watermelon.png'),
('葡萄', 'grape', 72, 6, '{"vitaminC": "3.2mg/100g", "fiber": "0.9g/100g", "calories": "69kcal/100g"}', '/images/grape.png'),
('草莓', 'strawberry', 48, 4, '{"vitaminC": "58.8mg/100g", "fiber": "2g/100g", "calories": "32kcal/100g"}', '/images/strawberry.png'),
('梨', 'pear', 168, 24, '{"vitaminC": "4.3mg/100g", "fiber": "3.1g/100g", "calories": "57kcal/100g"}', '/images/pear.png'),
('芒果', 'mango', 96, 12, '{"vitaminC": "36.4mg/100g", "fiber": "1.6g/100g", "calories": "60kcal/100g"}', '/images/mango.png'),
('袋装水果', 'fruitInBags', 336, NULL, NULL, '/images/bagged_fruit.png');
