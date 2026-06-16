// app.js
App({
  // 全局共享数据
  globalData: {
    userInfo: null,
    isLogin: false
  },

  onLaunch() {
    console.log('智慧冰箱小程序启动');
    // 启动时检查一次本地缓存
    const token = wx.getStorageSync('token');
    const userInfo = wx.getStorageSync('userInfo');
    
    if (token && userInfo) {
      this.globalData.isLogin = true;
      this.globalData.userInfo = userInfo;
      console.log('已检测到登录状态');
    }
  }
})