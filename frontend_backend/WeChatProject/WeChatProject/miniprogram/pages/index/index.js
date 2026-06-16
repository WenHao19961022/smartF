// pages/index/index.js
const { request } = require('../../utils/request');

Page({
  data: {
    isLogin: false,
    deviceSn: '',
    groupedList: [],    // 类别数据（直接接收后端返回的 data）
    totalInventory: 0,  // 在库总数量（算出来的总数）
    warningCount: 0,    // 临期提醒数量
    isFirstLoad: true,
    temperature: null,  // 冰箱温度（°C）
    humidity: null,     // 冰箱湿度（%）
    isOnline: null      // 设备在线状态（null=初始化, true=在线, false=离线）
  },

  onShow() {
    const token = wx.getStorageSync('token');
    const sn = wx.getStorageSync('deviceSn');
    this.setData({ isLogin: !!token, deviceSn: sn || '' });
    if (token && sn) {
      this.fetchData();
      this.fetchDeviceStatus();
    } else {
      this.setData({ groupedList: [], totalInventory: 0, warningCount: 0, temperature: null, humidity: null, isOnline: null });
    }
  },

  onAddAction() {
    wx.navigateTo({ url: this.data.isLogin ? '/pages/bind/bind' : '/pages/login/login' });
  },

  async fetchData() {
    if (!this.data.deviceSn) return;
    try {
      if (this.data.isFirstLoad) wx.showLoading({ title: '同步冰箱数据...', mask: true });
      wx.showNavigationBarLoading();
      
      // 请求后端接口
      const res = await request('/inventory/fruitStatistic?deviceSn=' + this.data.deviceSn);
      
      if (res.code === 200 && res.data) {
        let total = 0;
        let warnings = 0;

        // 核心：处理后端直接返回的数据
        const cleanedData = res.data.map(item => {
          // 处理图片路径斜杠问题
          if (item.imageUrl && !item.imageUrl.startsWith('/')) {
             item.imageUrl = '/' + item.imageUrl;
          }
          
          // 累加计算顶部统计大屏的数字
          total += (item.count || 0);
          if (item.warningSignal === true) {
            warnings += 1; // 记录有几类水果发出预警
          }

          return item;
        });

        this.setData({
          groupedList: cleanedData,
          totalInventory: total,      // 更新在库总数
          warningCount: warnings,     // 更新临期类别数
          isFirstLoad: false
        });

        // 体验优化：如果有预警，给一个轻微震动
        if (warnings > 0) wx.vibrateShort({ type: 'medium' });
      }
    } catch (e) {
      console.error("fetchData 请求失败:", e);
    } finally {
      wx.hideLoading(); wx.hideNavigationBarLoading(); wx.stopPullDownRefresh();
    }
  },

  // 🚨 点击卡片，跳转到 detail 页面
  goToDetail(e) {
    const fruitName = e.currentTarget.dataset.name;
    wx.navigateTo({
      url: `/pages/detail/detail?name=${fruitName}`
    });
  },

  // 拉取设备状态（温度 / 湿度 / 在线状态）
  async fetchDeviceStatus() {
    if (!this.data.deviceSn) return;
    try {
      const res = await request('/device/status?deviceSn=' + this.data.deviceSn);
      if (res.code === 200 && res.data) {
        // online: 0 = 离线，其他值视为在线
        this.setData({
          temperature: res.data.temperature,
          humidity: res.data.humidity,
          isOnline: res.data.online !== 0
        });
      }
    } catch (e) {
      console.error("设备状态获取失败:", e);
      // 失败不影响主流程展示
    }
  },

  onPullDownRefresh() {
    if (this.data.isLogin && this.data.deviceSn) {
      this.fetchData();
      this.fetchDeviceStatus();
    }
    else { wx.stopPullDownRefresh(); wx.showToast({ title: '请先绑定设备', icon: 'none' }); }
  }
});