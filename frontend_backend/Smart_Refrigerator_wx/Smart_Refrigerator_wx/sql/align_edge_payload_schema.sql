-- Align backend schema with edge MQTT snapshot payload.
-- Run this once on an existing smart_fridge database before receiving the new payload.

ALTER TABLE fridge_inventory
    ADD COLUMN IF NOT EXISTS batch_id VARCHAR(64) NULL COMMENT '快照批次/消息ID' AFTER id,
    ADD COLUMN IF NOT EXISTS track_id INT NULL COMMENT '硬件算法生成的水果追踪ID' AFTER device_id,
    ADD COLUMN IF NOT EXISTS fruit_code VARCHAR(20) NULL COMMENT '硬件水果类型代码' AFTER track_id,
    ADD COLUMN IF NOT EXISTS fresh_code VARCHAR(32) NULL COMMENT '硬件新鲜度原始值/标签' AFTER fruit_name,
    ADD COLUMN IF NOT EXISTS fresh_score INT NULL COMMENT '视觉新鲜度百分制分数' AFTER fresh_code,
    ADD COLUMN IF NOT EXISTS weight FLOAT NULL COMMENT '当前重量(g)' AFTER fresh_score,
    ADD COLUMN IF NOT EXISTS coordinate_x FLOAT DEFAULT 0 COMMENT '视觉坐标X，0-255' AFTER weight,
    ADD COLUMN IF NOT EXISTS coordinate_y FLOAT DEFAULT 0 COMMENT '视觉坐标Y，0-255' AFTER coordinate_x;

ALTER TABLE device_status
    ADD COLUMN IF NOT EXISTS batch_id VARCHAR(64) NULL COMMENT '最后一次库存快照批次ID' AFTER humidity;

INSERT INTO fruit_dict (name, category_code, shelf_life_whole, shelf_life_cut, nutrient, image_url)
SELECT '葡萄', 'grape', 96, 12, '{"vitaminC": "3.2mg", "fiber": "0.9g", "calories": "69kcal"}', 'https://img.example.com/grape.png'
FROM DUAL WHERE NOT EXISTS (SELECT 1 FROM fruit_dict WHERE category_code = 'grape');

INSERT INTO fruit_dict (name, category_code, shelf_life_whole, shelf_life_cut, nutrient, image_url)
SELECT '梨', 'pear', 168, 36, '{"vitaminC": "4.3mg", "fiber": "3.1g", "calories": "57kcal"}', 'https://img.example.com/pear.png'
FROM DUAL WHERE NOT EXISTS (SELECT 1 FROM fruit_dict WHERE category_code = 'pear');

INSERT INTO fruit_dict (name, category_code, shelf_life_whole, shelf_life_cut, nutrient, image_url)
SELECT '芒果', 'mango', 72, 12, '{"vitaminC": "36.4mg", "fiber": "1.6g", "calories": "60kcal"}', 'https://img.example.com/mango.png'
FROM DUAL WHERE NOT EXISTS (SELECT 1 FROM fruit_dict WHERE category_code = 'mango');

INSERT INTO fruit_dict (name, category_code, shelf_life_whole, shelf_life_cut, nutrient, image_url)
SELECT '袋装水果', 'fruitInBags', 336, NULL, NULL, 'https://img.example.com/bagged_fruit.png'
FROM DUAL WHERE NOT EXISTS (SELECT 1 FROM fruit_dict WHERE category_code = 'fruitInBags');
