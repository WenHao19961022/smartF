// pages/eco/eco.js
const { request } = require('../../utils/request');

Page({
  /**
   * 页面的初始数据结构 (与 WXML 严格绑定)
   */
  data: {
    isLoading: true,
    savedWeight: '0.0',   // 累计拯救食物 (kg)
    co2Reduced: '0.0',    // 减少碳排放 (kg)
    waterSaved: '0',      // 节约水资源 (L)
    treeLevel: 1,         // 果树等级
    treeStatus: '萌芽',    // 果树状态描述
    treeProgress: 0,      // 升级进度百分比
    nextLevelNeed: 5,     // 距离下一级还需要拯救的水果数
    saveLogs: []          // 拯救记录日志
  },

  /**
   * 生命周期函数--监听页面显示
   * 每次切换到这个 Tab 都会触发数据刷新
   */
  onShow() {
    this.fetchEcoData();
  },

  /**
   * 核心业务：拉取 ESG 环保统计数据
   */
  async fetchEcoData() {
    const deviceSn = wx.getStorageSync('deviceSn');
    
    // 如果未绑定设备，清空数据
    if (!deviceSn) {
      this.resetData();
      return;
    }

    try {
      wx.showNavigationBarLoading();
      
      // 🚀【真实后端对接预留口】
      // 比赛后期如果后端写了接口，把下面这几行注释打开即可：
      /*
      const res = await request('/eco/stats?deviceSn=' + deviceSn);
      if (res.code === 200 && res.data) {
        this.setData({ ...res.data, isLoading: false });
      }
      */

      // 💡【比赛路演专用 Mock 数据】
      // 模拟 800ms 的网络请求延迟，让录屏效果更逼真
      setTimeout(() => {
        this.setData({
          savedWeight: '1.8',
          co2Reduced: '4.5',
          waterSaved: '62',
          treeLevel: 3,
          treeStatus: '茂盛',
          treeProgress: 75,
          nextLevelNeed: 3,
          saveLogs: [
            { id: 1, text: '成功在过期前消耗了 2 个苹果', time: '今天 09:15' },
            { id: 2, text: '成功在过期前消耗了 1 盒草莓', time: '昨天 18:30' },
            { id: 3, text: '系统提醒避免了 1 把香蕉的浪费', time: '周二 12:00' }
          ],
          isLoading: false
        });
        wx.hideNavigationBarLoading();
        wx.stopPullDownRefresh();
      }, 800);

    } catch (error) {
      console.error('获取环保数据失败', error);
      wx.hideNavigationBarLoading();
      wx.stopPullDownRefresh();
    }
  },

  /**
   * 恢复初始状态
   */
  resetData() {
    this.setData({
      savedWeight: '0.0', co2Reduced: '0.0', waterSaved: '0',
      treeLevel: 1, treeStatus: '未激活', treeProgress: 0, nextLevelNeed: 0,
      saveLogs: []
    });
  },

  /**
   * 监听用户下拉刷新动作
   */
  onPullDownRefresh() {
    this.fetchEcoData();
  },

  /**
   * 用户分享配置：比赛评委体验时，分享出去的卡片也要有 ESG 格局
   */
  onShareAppMessage() {
    return {
      title: `我已经通过智慧冰箱拯救了 ${this.data.savedWeight}kg 食物，一起来保护地球吧！`,
      path: '/pages/eco/eco',
      imageUrl: '/images/eco_share.png' // 建议放一张精美的环保主题图
    };
  }
});