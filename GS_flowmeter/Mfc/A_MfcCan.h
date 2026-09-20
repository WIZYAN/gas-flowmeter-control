/* Created on: 2026年9月20日，Author: CI */
#ifndef MFC_A_MFC_CAN_H_
#define MFC_A_MFC_CAN_H_
#include "A_EX201.h"
#include "F_MfcCan.h"

#define A_MFCCAN_LOCAL_ADDRESS (1U) // MCU在下行总线使用Header/1，与上位机CAN0隔离
#define A_MFCCAN_IDENTITY (0x4D464301UL) // 本项目MFC配置标识，非KOFLOC官方协议
#define A_MFCCAN_PROFILE_VERSION (1UL) // 下行参数表版本，与上位机参数表分别维护
#define A_MFCCAN_RESPONSE_MS (200U) // 请求提交至完整应答的总期限
#define A_MFCCAN_QUIET_MS (250U) // 错误后清旧帧并静默，需对端满足100ms响应上限
#define A_MFCCAN_ACTUAL (0x0000U) // float32，瞬时流量
#define A_MFCCAN_CONFIRMED (0x0001U) // float32，当前数字设定流量
#define A_MFCCAN_DEVICE_ID (0x0100U) // uint32，设备配置标识
#define A_MFCCAN_FULL_SCALE (0x0101U) // uint32，满刻度尾数1～9999
#define A_MFCCAN_DECIMAL (0x0102U) // uint32，小数位0～3
#define A_MFCCAN_UNIT (0x0103U) // uint32，0=sccm，1=slm，基准条件由设备配置一致保证
#define A_MFCCAN_SOURCE (0x0104U) // uint32，0数字，1模拟
#define A_MFCCAN_SETTING (0x0105U) // uint32，内部阀数字设定0～2
#define A_MFCCAN_VALVE (0x0106U) // uint32，内部阀实际状态0～3
#define A_MFCCAN_ALARM (0x0107U) // uint32，报警位0～7，语义同现有EX201缓存
#define A_MFCCAN_VERSION (0x0108U) // uint32，下行参数表版本
#define A_MFCCAN_TARGET (0x0200U) // float32，读写数字流量设定；本版唯一写入口

typedef struct
{
    F_MfcCan_Context *p_transport; // 同一MfcTask独占的CAN驱动，初始化前绑定
    uint32_t initialized; // 后端已初始化
    uint32_t state; // 0空闲，1等待应答，2结果待取
    uint32_t value; // 应答数据位模式
    uint32_t rejected_frames; // 格式或关联字段不匹配的帧
    uint32_t device_error; // 写应答中的对端原始错误码
    uint16_t parameter; // 当前参数地址
    uint8_t node; // 当前FLOW节点1～6
    uint8_t function; // 请求READ或WRITE
    TickType_t started_tick; // 本事务期限起点
    A_EX201_Result result; // 沿用现有MFC公共事务结果码，名称保留以减少改动
} A_MfcCan_Context;

/*
 * 说明：初始化下行CAN后端；调用前必须停止UART事务
 * 输入：p_context 已绑定传输的长期状态
 * 输出：A_EX201_Result 公共事务结果
 */
A_EX201_Result A_MfcCan_Initialize(A_MfcCan_Context *p_context);
/*
 * 说明：停止CAN后端并丢弃结果；成功后才能切换RS485
 * 输入：p_context 状态
 * 输出：A_EX201_Result 公共事务结果
 */
A_EX201_Result A_MfcCan_Stop(A_MfcCan_Context *p_context);
/*
 * 说明：排队一笔单参数CAN_USER事务，不访问SPI，不自动重试写入
 * 输入：p_context 状态，node 节点1～6，parameter 地址，write 非0写入，value 原始数据，now 节拍
 * 输出：A_EX201_Result 启动结果
 */
A_EX201_Result A_MfcCan_Start(A_MfcCan_Context *p_context, uint16_t node, uint16_t parameter,
                             uint32_t write, uint32_t value, TickType_t now);
/*
 * 说明：轮询SPI并核对校验、来源、目标、功能、参数和数量；无关帧不能确认链路
 * 输入：p_context 状态，now 节拍
 * 输出：无
 */
void A_MfcCan_Process(A_MfcCan_Context *p_context, TickType_t now);
/*
 * 说明：取出通用事务结果，成功时返回原始位模式
 * 输入：p_context 状态，p_value 输出值，可为NULL
 * 输出：A_EX201_Result 结果
 */
A_EX201_Result A_MfcCan_GetResult(A_MfcCan_Context *p_context, uint32_t *p_value);
/*
 * 说明：校验并更新一个元数据字段，也用于设备标识和版本确认
 * 输入：p_context 状态，p_info 本通道元数据
 * 输出：A_EX201_Result 结果
 */
A_EX201_Result A_MfcCan_GetInfo(A_MfcCan_Context *p_context, A_EX201_DeviceInfo *p_info);
/*
 * 说明：按本通道小数位把收到的float32转换为公共缓存尾数
 * 输入：p_context 状态，p_info 本通道元数据，p_mantissa 输出尾数
 * 输出：A_EX201_Result 结果
 */
A_EX201_Result A_MfcCan_GetFlow(A_MfcCan_Context *p_context, const A_EX201_DeviceInfo *p_info, int32_t *p_mantissa);
#endif
