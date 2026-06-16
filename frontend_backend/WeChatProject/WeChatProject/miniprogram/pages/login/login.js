const { request } = require('../../utils/request');

Page({
  data: {
    isAgreed: false
  },

  // 勾选状态改变
  onAgree(e) {
    this.setData({ isAgreed: e.detail.value.length > 0 });
  },

  // 微信登录逻辑
  handleWxLogin() {
    if (!this.data.isAgreed) {
      wx.showToast({ title: '请先勾选并同意协议', icon: 'none' });
      return;
    }

    wx.showLoading({ title: '安全连接中...', mask: true });
    
    wx.login({
      success: async (loginRes) => {
        if (loginRes.code) {
          // 调试：打印微信生成的临时 Code
          console.log('1. 微信生成的临时Code:', loginRes.code);

          try {
            // 调用 Java 后端接口
            const res = await request('/auth/login', 'POST', { code: loginRes.code });
            
            // 调试：查看后端返回的完整数据
            console.log('2. 后端返回结果:', res);

            if (res.code === 200) {
              // 【关键修改】：根据你后端返回的结构解构数据
              const { token, userId, nickname, avatar, deviceSnList } = res.data;

              console.log('3. 登录成功！生成的Token是:', token);
              console.log('4. 该用户绑定的设备列表:', deviceSnList);

              // 1. 存入 Token（用于后续所有请求的身份验证）
              wx.setStorageSync('token', token);

              // 2. 存入用户信息（用于“我的”页面展示）
              wx.setStorageSync('userInfo', {
                userId: userId,
                nickName: nickname || '微信用户',
                avatarUrl: avatar || '/images/icons/default_avatar.png' // 兜底头像
              });

              // 3. 【核心优化】：如果用户之前绑定过设备，自动存入第一个设备SN
              // 这解决了你说的“重新登录后不显示水果”的问题，因为首页会读取这个缓存去查库存
              if (deviceSnList && deviceSnList.length > 0) {
                wx.setStorageSync('deviceSn', deviceSnList[0]);
                console.log('5. 已自动找回并缓存设备SN:', deviceSnList[0]);
              } else {
                // 如果没有绑定过，确保清空旧的设备缓存，防止数据混乱
                wx.removeStorageSync('deviceSn');
              }

              wx.showToast({ title: '登录成功', icon: 'success' });
              
              // 4. 登录成功后，建议使用 reLaunch 或 switchTab 刷新全局状态
              // 如果想直接返回上一页，使用 navigateBack
              setTimeout(() => {
                wx.switchTab({
                  url: '/pages/index/index',
                });
              }, 1200);

            } else {
              console.error('登录失败原因:', res.message);
              wx.showToast({ title: res.message || '登录失败', icon: 'none' });
            }
          } catch (err) {
            console.error('网络请求异常:', err);
            wx.showToast({ title: '服务器连接异常', icon: 'none' });
          } finally {
            wx.hideLoading();
          }
        }
      },
      fail: (err) => {
        wx.hideLoading();
        wx.showToast({ title: '微信登录失败', icon: 'none' });
        console.error('wx.login fail:', err);
      }
    });
  }
});