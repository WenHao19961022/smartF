package com.smartfridge.vo;

import lombok.*;

/**
 * @Author:ZhangXinqi
 * @Description:
 * @Date: Created in 23:59 2026/4/23
 */
@Data
@Setter
@Getter
@NoArgsConstructor
@AllArgsConstructor
public class DeviceStatusVO {
    private String deviceSn;

    /** 0-离线, 1-在线 */
    private Integer online;

    private Float temperature;

    private Float humidity;
}
