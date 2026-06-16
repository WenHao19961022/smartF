-- =====================================================
-- 袋装水果字典数据
-- 适用场景: 硬件无法识别袋装内的具体水果, 只能识别"袋装"这一类型
-- 保质期: 14天(336小时)兜底, 实际值会按重量在代码中对数弱修正
-- =====================================================

-- 幂等插入: 避免重复执行时报 UNIQUE 冲突
INSERT INTO fruit_dict (name, category_code, shelf_life_whole, shelf_life_cut, nutrient, image_url)
SELECT '袋装水果', 'fruitInBags', 336, NULL, NULL, 'https://img.example.com/bagged_fruit.png'
FROM DUAL
WHERE NOT EXISTS (
    SELECT 1 FROM fruit_dict WHERE category_code = 'fruitInBags'
);
