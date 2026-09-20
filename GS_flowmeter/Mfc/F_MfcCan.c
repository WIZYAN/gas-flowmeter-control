/* Created on: 2026年9月20日，Author: CI */
#include "F_MfcCan.h"
#include <string.h>

/*
 * 说明：限制接收目标Header和来源FLOW，节点及功能码在业务层进一步核对
 * 输入：p_context 状态，local_address 本地节点
 * 输出：F_MfcCan_Result 结果
 */
F_MfcCan_Result F_MfcCan_Initialize(F_MfcCan_Context *p_context, uint8_t local_address)
{
    uint32_t filter = ((uint32_t) Type_Header << 19U) | ((uint32_t) local_address << 12U) |
                      ((uint32_t) Type_FLOW << 7U); // 与CAN_USER的29位ID布局一致
    if (local_address > 127U) { return H_MFCCAN_ERROR; }
    return H_MfcCan_Initialize(p_context, filter, 0x00FFFF80UL);
}
/*
 * 说明：推进硬件收发
 * 输入：p_context 状态，now 当前节拍
 * 输出：无
 */
void F_MfcCan_Process(F_MfcCan_Context *p_context, TickType_t now)
{
    H_MfcCan_Process(p_context, now);
}
/*
 * 说明：复制编码帧到硬件待发槽
 * 输入：p_context 状态，p_frame 帧
 * 输出：F_MfcCan_Result 结果
 */
F_MfcCan_Result F_MfcCan_Send(F_MfcCan_Context *p_context, const F_CanUser_Frame *p_frame)
{
    H_MfcCan_Frame g_frame = {0}; // 硬件帧副本
    if (NULL == p_frame) { return H_MFCCAN_ERROR; }
    g_frame.id = p_frame->id;
    memcpy(g_frame.data, p_frame->data, sizeof(g_frame.data));
    return H_MfcCan_Send(p_context, &g_frame);
}
/*
 * 说明：取出硬件帧交给协议解析
 * 输入：p_context 状态，p_frame 输出帧
 * 输出：F_MfcCan_Result 结果
 */
F_MfcCan_Result F_MfcCan_Receive(F_MfcCan_Context *p_context, F_CanUser_Frame *p_frame)
{
    H_MfcCan_Frame g_frame = {0}; // 硬件帧
    H_MfcCan_Result result = H_MFCCAN_ERROR; // 读取结果
    if (NULL == p_frame) { return H_MFCCAN_ERROR; }
    result = H_MfcCan_Receive(p_context, &g_frame);
    if (result == H_MFCCAN_OK)
    {
        p_frame->id = g_frame.id;
        memcpy(p_frame->data, g_frame.data, sizeof(p_frame->data));
    }
    return result;
}
/*
 * 说明：查询硬件发送状态
 * 输入：p_context 状态
 * 输出：F_MfcCan_Result 结果
 */
F_MfcCan_Result F_MfcCan_GetTransmitState(const F_MfcCan_Context *p_context)
{
    return H_MfcCan_GetTransmitState(p_context);
}
/*
 * 说明：停止CAN控制器
 * 输入：p_context 状态
 * 输出：F_MfcCan_Result 结果
 */
F_MfcCan_Result F_MfcCan_Stop(F_MfcCan_Context *p_context)
{
    return H_MfcCan_Stop(p_context);
}
