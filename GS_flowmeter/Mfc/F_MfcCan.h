/* Created on: 2026年9月20日，Author: CI */
#ifndef MFC_F_MFC_CAN_H_
#define MFC_F_MFC_CAN_H_
#include "H_MfcCan.h"
#include "F_CanUser.h"

typedef H_MfcCan_Context F_MfcCan_Context; // 直接使用硬件状态，避免嵌套包装
typedef H_MfcCan_Result F_MfcCan_Result; // 传输结果沿用硬件枚举

/*
 * 说明：初始化下行CAN传输，过滤器接收发给本控制器的FLOW消息
 * 输入：p_context 状态，local_address 本控制器在下行总线的Header地址
 * 输出：F_MfcCan_Result 结果
 */
F_MfcCan_Result F_MfcCan_Initialize(F_MfcCan_Context *p_context, uint8_t local_address);
/*
 * 说明：周期服务MCP2515；仅MfcTask调用
 * 输入：p_context 状态，now 当前节拍
 * 输出：无
 */
void F_MfcCan_Process(F_MfcCan_Context *p_context, TickType_t now);
/*
 * 说明：提交一帧CAN_USER待发送数据，不访问SPI
 * 输入：p_context 状态，p_frame 编码帧
 * 输出：F_MfcCan_Result 结果
 */
F_MfcCan_Result F_MfcCan_Send(F_MfcCan_Context *p_context, const F_CanUser_Frame *p_frame);
/*
 * 说明：读取一帧完整数据
 * 输入：p_context 状态，p_frame 输出帧
 * 输出：F_MfcCan_Result 结果
 */
F_MfcCan_Result F_MfcCan_Receive(F_MfcCan_Context *p_context, F_CanUser_Frame *p_frame);
/*
 * 说明：查询发送状态
 * 输入：p_context 状态
 * 输出：F_MfcCan_Result 结果
 */
F_MfcCan_Result F_MfcCan_GetTransmitState(const F_MfcCan_Context *p_context);
/*
 * 说明：停止下行CAN并丢弃旧帧
 * 输入：p_context 状态
 * 输出：F_MfcCan_Result 结果
 */
F_MfcCan_Result F_MfcCan_Stop(F_MfcCan_Context *p_context);
#endif
