const { request } = require('../../utils/request');

Page({
  data: {
    searchVal: '',
    selectedSn: '',
    allDevices: [
      { name: 'BCD-228WB A', sn: 'SN-10001' }
      //{ name: 'BCD-248WB A', sn: 'SN-002' },
      //{ name: 'BCD-226SK A', sn: 'SN-AIR-01' }
    ],
    filteredList: []
  },

  onSearchChange(e) {
    const val = e.detail.trim();
    if (!val) {
      this.setData({ searchVal: '', filteredList: [], selectedSn: '' });
      return;
    }

    const filtered = this.data.allDevices.filter(item => 
      item.name.toUpperCase().includes(val.toUpperCase()) || 
      item.sn.toUpperCase().includes(val.toUpperCase())
    );

    this.setData({ searchVal: val, filteredList: filtered });
  },

  selectDevice(e) {
    const sn = e.currentTarget.dataset.sn;
    this.setData({ selectedSn: sn });
    wx.vibrateShort({ type: 'light' });
  },

  async confirmBind() {
    if (!this.data.selectedSn) return;
    wx.showLoading({ title: '正在绑定' });
    try {
      const userInfo = wx.getStorageSync('userInfo');
      // const res = await request('/device/bind', 'POST', {
      //   openid: userInfo.openid,
      //   deviceSn: this.data.selectedSn
      // });
      // 拼接 query 参数
      const url = `/device/bind?deviceSn=${this.data.selectedSn}`;
      // 如果还需要 openid，可以继续拼接 &openid=...
      // 但注意：openid 敏感，通常走 header 或 body，不过这里按后端要求来
      const res = await request(url, 'POST', null); // 无 body 或空 body

      if (res.code === 200) {
        wx.setStorageSync('deviceSn', this.data.selectedSn);
        wx.showToast({ title: '绑定成功', icon: 'success' });
        setTimeout(() => {
          wx.reLaunch({ url: '/pages/index/index' });
        }, 1500);
      }
    } catch (e) {
      wx.showToast({ title: '服务异常', icon: 'none' });
    } finally {
      wx.hideLoading();
    }
  }
});