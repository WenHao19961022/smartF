package com.smartfridge.mapper;

import com.baomidou.mybatisplus.core.mapper.BaseMapper;
import com.smartfridge.entity.SysUser;
import org.apache.ibatis.annotations.Param;

import java.util.List;

/**
 * 系统用户 Mapper
 */
public interface SysUserMapper extends BaseMapper<SysUser> {

    SysUser getByOpenid(@Param("openid") String openid);

    /**
     * 查询开启了推送通知且绑定了邮箱的用户
     */
    List<SysUser> getSendEmailUserList();


}
