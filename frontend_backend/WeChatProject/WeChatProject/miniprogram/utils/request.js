const BASE_URL = 'http://101.34.239.30:8080';

/**
 * 通用请求工具类封装 (增强版)
 */
const request = (url, method = 'GET', data = {}) => {
  const token = wx.getStorageSync('token');

  return new Promise((resolve, reject) => {
    wx.request({
      url: BASE_URL + '/api' + url,
      method: method,
      data: data,
      timeout: 10000,
      header: {
        'Authorization': token ? 'Bearer ' + token : '',
        'Content-Type': 'application/json'
      },
      success: (res) => {
        // --- 核心逻辑：处理 401 登录失效（如后端删库或Token过期） ---
        if (res.statusCode === 401 || (res.data && res.data.code === 401)) {
          console.error(">>> 授权验证失败，正在执行全局清理...");
          
          // 1. 彻底清除本地身份缓存
          wx.removeStorageSync('token');
          wx.removeStorageSync('userInfo');
          wx.removeStorageSync('deviceSn');
          wx.removeStorageSync('pushEnabled');
          wx.removeStorageSync('phoneNumber');

          // 2. 强制实时刷新当前页面 UI 状态
          const pages = getCurrentPages();
          if (pages.length > 0) {
            const currentPage = pages[pages.length - 1];
            // 确保页面拥有对应变量才执行，避免报错
            if (currentPage) {
              currentPage.setData({
                isLogin: false,
                deviceSn: '',
                inventoryList: [],
                warningCount: 0
              });
            }
          }

          wx.hideLoading();
          wx.showModal({
            title: '安全提示',
            content: '您的登录状态已失效或账号不存在，请重新登录。',
            showCancel: false,
            confirmText: '去登录'
          });

          reject(new Error('Login Expired'));
          return;
        }

        // --- 业务请求成功 ---
        if (res.statusCode === 200) {
          resolve(res.data);
        } else {
          wx.showToast({ title: '服务器连接失败', icon: 'none' });
          reject(res);
        }
      },
      fail: (err) => {
        console.error('>>> 网络层错误:', url, err.errMsg);
        wx.hideLoading();
        wx.showToast({ title: '无法连接到公网服务器', icon: 'none' });
        reject(err);
      }
    });
  });
};

module.exports = { request };