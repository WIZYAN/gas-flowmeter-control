/*
 * H_HostCan.c
 *
 * Created on: 2026年9月12日
 * Author: YXZ
 */

#include "H_HostCan.h"

#include "HostCanStack.h"

/*
 * 说明：初始化上位机侧CAN硬件驱动
 * 输入：无
 * 输出：fsp_err_t FSP驱动初始化结果
 */
fsp_err_t H_HostCan_Initialize(void)
{
    return g_can0.p_api->open(g_can0.p_ctrl, g_can0.p_cfg);
}

/*
 * 说明：处理上位机侧CAN接收、发送及错误事件
 * 输入：p_args FSP CAN回调参数
 * 输出：无
 */
void H_HostCan_Callback(can_callback_args_t *p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
}
