/*
 * Created on: 2026年9月14日
 * Author: YXZ
 */
#ifndef CAN_F_HOST_CAN_H_
#define CAN_F_HOST_CAN_H_
#include "H_HostCan.h"
#include "F_CanUser.h"

typedef enum
{
    F_HOSTCAN_OK = 0, // 完成
    F_HOSTCAN_EMPTY,  // 接收为空
    F_HOSTCAN_BUSY,   // 发送忙
    F_HOSTCAN_ERROR   // 传输异常
} F_HostCan_Result;
typedef H_HostCan_Context F_HostCan_Context; // 传输层直接使用硬件状态，不再增加仅含hardware的包装结构

/*
 * 说明：初始化传输
 * 输入：p_context 上下文
 * 输出：F_HostCan_Result 操作结果
 */
F_HostCan_Result F_HostCan_Initialize(F_HostCan_Context *p_context);

/*
 * 说明：查询发送状态
 * 输入：p_context 上下文
 * 输出：F_HostCan_Result 操作结果
 */
F_HostCan_Result F_HostCan_GetTransmitState(F_HostCan_Context *p_context);

/*
 * 说明：恢复传输
 * 输入：p_context 上下文
 * 输出：F_HostCan_Result 操作结果
 */
F_HostCan_Result F_HostCan_Recover(F_HostCan_Context *p_context);

/*
 * 说明：接收完整协议帧
 * 输入：p_context 上下文，p_frame 帧数据
 * 输出：F_HostCan_Result 操作结果
 */
F_HostCan_Result F_HostCan_Receive(F_HostCan_Context *p_context, F_CanUser_Frame *p_frame);

/*
 * 说明：异步发送协议帧
 * 输入：p_context 上下文，p_frame 帧数据
 * 输出：F_HostCan_Result 操作结果
 */
F_HostCan_Result F_HostCan_Send(F_HostCan_Context *p_context, const F_CanUser_Frame *p_frame);

#endif
