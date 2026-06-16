const { request } = require('../../utils/request');

function maskEmail(email) {
  if (!email || !email.includes('@')) return email || '';
  const [user, domain] = email.split('@');
  if (user.length <= 2) return user + '***@' + domain;
  return user.slice(0, 1) + '****' + user.slice(-1) + '@' + domain;
}

Page({
  data: {
    isLogin: false,
    deviceSn: '',
    pushEnabled: false,    
    email: '',             
    emailDisplay: '点击绑定', // 🚨 用于界面强制显示的纯净变量
    userInfo: {}
  },

  handleNavToLogin() {
    wx.navigateTo({ url: '/pages/login/login' });
  },

  onShow() {
    this.refreshStatus();
  },

  async refreshStatus() {
    if (this._loggingOut) return;
    const token = wx.getStorageSync('token');
    const sn = wx.getStorageSync('deviceSn');

    this.setData({
      isLogin: !!token,
      deviceSn: sn || ''
    });

    if (token) {
      await this.fetchUserInfo();
    } else {
      // 🚨 没有 token 时，彻底清空
      this.setData({ email: '', emailDisplay: '点击绑定', pushEnabled: false });
    }
  },

  async fetchUserInfo() {
    try {
      const res = await request('/user/info', 'GET');
      if (!wx.getStorageSync('token')) return;

      if (res.code === 200 && res.data) {
        const rawEmail = res.data.email || '';
        
        let backendStatus = false;
        if (res.data.pushEnabled !== undefined) {
          backendStatus = res.data.pushEnabled;
        } else if (res.data.smsEnabled !== undefined) {
          backendStatus = res.data.smsEnabled;
        }

        const isPushOn = backendStatus === true || backendStatus === 1 || backendStatus === 'true';

        this.setData({
          email: rawEmail,
          // 🚨 有邮箱就显示打码邮箱，没有就显示文字
          emailDisplay: rawEmail ? maskEmail(rawEmail) : '点击绑定',
          pushEnabled: isPushOn, 
          userInfo: res.data
        });

        wx.setStorageSync('email', rawEmail);
        wx.setStorageSync('pushEnabled', isPushOn);
      }
    } catch (err) {
      console.error('获取用户信息失败', err);
    }
  },

  async onSmsToggle({ detail }) {
    if (!this.data.isLogin) {
      wx.showToast({ title: '请先登录', icon: 'none' });
      return;
    }
    
    if (!this.data.email) {
      wx.showToast({ title: '请先绑定邮箱', icon: 'none' });
      this.setData({ pushEnabled: false }); 
      return;
    }

    const oldStatus = !detail; 
    this.setData({ pushEnabled: detail });

    try {
      const res = await request('/user/push-setting', 'POST', {
        pushEnabled: detail, 
        smsEnabled: detail   
      });

      if (!res || res.code === 200 || res === '') {
        wx.setStorageSync('pushEnabled', detail);
        wx.showToast({ title: detail ? '已开启邮件预警' : '预警已关闭', icon: 'success' });
      } else {
        this.setData({ pushEnabled: oldStatus });
        wx.showToast({ title: res.message || '操作失败', icon: 'none' });
      }
    } catch (err) {
      console.error('推送设置接口报错:', err);
      this.setData({ pushEnabled: oldStatus });
      wx.showToast({ title: '网络异常', icon: 'none' });
    }
  },

  async onBindPhone() {
    if (!this.data.isLogin) {
      wx.showToast({ title: '请先登录', icon: 'none' });
      return;
    }

    wx.showModal({
      title: '绑定预警邮箱',
      editable: true,
      placeholderText: '请输入常用邮箱地址',
      success: async (res) => {
        if (res.confirm && res.content) {
          const inputEmail = res.content.trim();
          const emailReg = /^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$/;
          if (!emailReg.test(inputEmail)) {
            wx.showToast({ title: '邮箱格式不正确', icon: 'none' });
            return;
          }

          wx.showLoading({ title: '正在同步...', mask: true });
          try {
            const result = await request('/user/update-email', 'POST', { email: inputEmail });
            if (result.code === 200) {
              this.setData({
                email: inputEmail,
                emailDisplay: maskEmail(inputEmail) // 🚨 绑定成功后展示打码邮箱
              });
              wx.setStorageSync('email', inputEmail);
              wx.showToast({ title: '绑定成功', icon: 'success' });
              await this.fetchUserInfo();
            }
          } catch (err) {
            wx.showToast({ title: '绑定失败', icon: 'none' });
          } finally {
            wx.hideLoading();
          }
        }
      }
    });
  },

  onLogout() {
    wx.showModal({
      title: '提示',
      content: '确定退出登录吗？',
      confirmColor: '#eb2f06',
      success: (res) => {
        if (res.confirm) {
          this._loggingOut = true;
          wx.clearStorageSync();
          this.setData({
            isLogin: false,
            deviceSn: '',
            email: '',
            emailDisplay: '点击绑定', // 🚨 强制重置
            pushEnabled: false,
            userInfo: {}
          }, () => {
            this._loggingOut = false;
            wx.showToast({ title: '已安全退出', icon: 'none' });
          });
        }
      }
    });
  }
});