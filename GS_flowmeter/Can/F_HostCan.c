/*
 * Created on: 2026年9月14日
 * Author: YXZ
 */
#include "F_HostCan.h"
#include <stddef.h>
#include <string.h>

/*
 * 说明：转换硬件层结果
 * 输入：result 硬件结果
 * 输出：F_HostCan_Result 功能层结果
 */
static F_HostCan_Result F_HostCan_MapResult(H_HostCan_Result result)
{
    switch (result)
    {
        case H_HOSTCAN_OK:
            return F_HOSTCAN_OK;
        case H_HOSTCAN_EMPTY:
            return F_HOSTCAN_EMPTY;
        case H_HOSTCAN_BUSY:
            return F_HOSTCAN_BUSY;
        default:
            return F_HOSTCAN_ERROR;
    }
}

/*
 * 说明：初始化传输
 * 输入：p_context 上下文
 * 输出：F_HostCan_Result 操作结果
 */
F_HostCan_Result F_HostCan_Initialize(F_HostCan_Context *p_context)
{
    if (NULL == p_context)
    {
        return F_HOSTCAN_ERROR;
    }
    return F_HostCan_MapResult(H_HostCan_Initialize(p_context));
}

/*
 * 说明：查询发送状态
 * 输入：p_context 上下文
 * 输出：F_HostCan_Result 操作结果
 */
F_HostCan_Result F_HostCan_GetTransmitState(F_HostCan_Context *p_context)
{
    if (NULL == p_context)
    {
        return F_HOSTCAN_ERROR;
    }
    return F_HostCan_MapResult(H_HostCan_GetTransmitState(p_context));
}

/*
 * 说明：恢复传输
 * 输入：p_context 上下文
 * 输出：F_HostCan_Result 操作结果
 */
F_HostCan_Result F_HostCan_Recover(F_HostCan_Context *p_context)
{
    if (NULL == p_context)
    {
        return F_HOSTCAN_ERROR;
    }
    return F_HostCan_MapResult(H_HostCan_Recover(p_context));
}

/*
 * 说明：接收完整协议帧
 * 输入：p_context 上下文，p_frame 帧数据
 * 输出：F_HostCan_Result 操作结果
 */
F_HostCan_Result F_HostCan_Receive(F_HostCan_Context *p_context, F_CanUser_Frame *p_frame)
{
    H_HostCan_Frame g_frame = {0}; // 硬件帧
    H_HostCan_Result result = H_HOSTCAN_ERROR; // 接收结果
    if ((NULL == p_context) || (NULL == p_frame))
    {
        return F_HOSTCAN_ERROR;
    }
    result = H_HostCan_Receive(p_context, &g_frame);
    if (H_HOSTCAN_OK == result)
    {
        p_frame->id = g_frame.id;
        memcpy(p_frame->data, g_frame.data, 8U);
    }
    return F_HostCan_MapResult(result);
}

/*
 * 说明：异步发送协议帧
 * 输入：p_context 上下文，p_frame 帧数据
 * 输出：F_HostCan_Result 操作结果
 */
F_HostCan_Result F_HostCan_Send(F_HostCan_Context *p_context, const F_CanUser_Frame *p_frame)
{
    H_HostCan_Frame g_frame = {0}; // 硬件帧
    if ((NULL == p_context) || (NULL == p_frame))
    {
        return F_HOSTCAN_ERROR;
    }
    g_frame.id = p_frame->id;
    memcpy(g_frame.data, p_frame->data, 8U);
    return F_HostCan_MapResult(H_HostCan_Send(p_context, &g_frame));
}


