ALTER TABLE device_status
    ADD COLUMN IF NOT EXISTS vision_status INT DEFAULT 0 COMMENT '视觉健康状态，0=正常' AFTER batch_id,
    ADD COLUMN IF NOT EXISTS vision_error_time DATETIME NULL COMMENT '最后视觉错误时间' AFTER vision_status;
