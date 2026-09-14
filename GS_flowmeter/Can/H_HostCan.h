/*
 * H_HostCan.h
 *
 * Created on: 2026年9月12日
 * Author: YXZ
 */

#ifndef CAN_H_HOST_CAN_H_
#define CAN_H_HOST_CAN_H_

#include "r_can_api.h"

/*
 * 说明：初始化上位机侧CAN硬件驱动
 * 输入：无
 * 输出：fsp_err_t FSP驱动初始化结果
 */
fsp_err_t H_HostCan_Initialize(void);

/*
 * 说明：处理上位机侧CAN接收、发送及错误事件
 * 输入：p_args FSP CAN回调参数
 * 输出：无
 */
void H_HostCan_Callback(can_callback_args_t *p_args);

#endif /* CAN_H_HOST_CAN_H_ */
