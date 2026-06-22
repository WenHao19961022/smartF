// pages/detail/detail.js
const { request } = require('../../utils/request');

function clamp(num, min, max) {
  return Math.max(min, Math.min(max, num));
}

function normalizeVisionCoordinate(value) {
  const n = Number(value);
  if (!Number.isFinite(n)) return 50;
  if (n >= 0 && n <= 1) return clamp(n * 100, 4, 96);
  return clamp((n / 255) * 100, 4, 96);
}

function buildLocationLabel(leftPct, topPct) {
  const level = topPct < 33 ? '上层' : (topPct < 66 ? '中层' : '下层');
  const side = leftPct < 33 ? '左侧' : (leftPct < 66 ? '中部' : '右侧');
  return `${level} · ${side}`;
}

Page({
  data: {
    fruitName: '苹果',
    fruitInstances: []
  },

  onLoad(options) {
    if (options.name) {
      this.setData({ fruitName: options.name });
    }
    // 发起真实网络请求
    this.fetchFruitLocations();
  },

  async fetchFruitLocations() {
    const deviceSn = wx.getStorageSync('deviceSn');
    if (!deviceSn) return;

    wx.showLoading({ title: '扫描空间系...', mask: true });
    
    try {
      // 请求后端库存列表
      const res = await request('/inventory/list?deviceSn=' + deviceSn);

      // 防御：响应格式校验，避免 null.code 之类导致 [object Object]
      if (!res || res.code !== 200 || !Array.isArray(res.data)) {
        console.error("响应格式异常:", JSON.stringify(res));
        wx.showToast({ title: '数据格式异常', icon: 'none' });
        return;
      }

      // 1. 过滤出当前点击的这类水果（比如只保留苹果）
      const targetFruits = res.data.filter(item => item.fruitName === this.data.fruitName);

      // 2. 数据清洗与映射
      const letters = ['A', 'B', 'C', 'D', 'E', 'F', 'G']; // 同类水果编号
      const mappedData = targetFruits.map((item, index) => {
        // fruitCode === "7" 表示袋装/遮挡袋类目标（与边缘端 FruitType::PlasticBag 对齐）
        const isBagged = String(item.fruitCode) === "7" || item.fruitCode === "fruitInBags";
        item.isBagged = isBagged;
        item.displayName = isBagged
          ? `袋装 ${letters[index] || index}`
          : `${item.fruitName} ${letters[index] || index}`;

        // 处理时间格式：去掉 'T'，只保留到分钟
        if (item.expireTime) {
          item.expireTime = item.expireTime.replace('T', ' ').substring(0, 16);
        } else {
          item.expireTime = '计算中...';
        }

        const rawX = Number(item.coordinateX || 0);
        const rawY = Number(item.coordinateY || 0);
        const leftPct = normalizeVisionCoordinate(rawX);
        const topPct = normalizeVisionCoordinate(rawY);

        item.rawX = Number.isFinite(rawX) ? Math.round(rawX) : 0;
        item.rawY = Number.isFinite(rawY) ? Math.round(rawY) : 0;
        item.leftPct = Math.round(leftPct);
        item.topPct = Math.round(topPct);
        item.locationLabel = buildLocationLabel(leftPct, topPct);

        // 确保是布尔值
        item.isExpiring = !!item.isExpiring;

        return item;
      });

      this.setData({ fruitInstances: mappedData });
    } catch (e) {
      // 把对象错误转成可读字符串，避免 IDE 显示 [object Object]
      let detail = '';
      if (e) {
        if (e.errMsg) detail = e.errMsg;
        else if (e.statusCode) detail = `HTTP ${e.statusCode}`;
        else if (e.message) detail = e.message;
        else { try { detail = JSON.stringify(e); } catch (_) { detail = String(e); } }
      }
      console.error("空间系扫描失败:", detail);
      wx.showToast({ title: '数据获取失败', icon: 'none' });
    } finally {
      wx.hideLoading();
    }
  }
});
