/*
 * H_HostCan.h
 *
 * Created on: 2026年9月12日
 * Author: YXZ
 */

#ifndef CAN_H_HOST_CAN_H_
#define CAN_H_HOST_CAN_H_

#include <stdint.h>
#include "r_can_api.h"

#define H_HOSTCAN_RX_CAPACITY (16U) // 中断接收队列容量
#define H_HOSTCAN_TX_MAILBOX (0U) // FSP发送邮箱

typedef enum
{
    H_HOSTCAN_OK = 0, // 完成
    H_HOSTCAN_EMPTY,  // 队列为空
    H_HOSTCAN_BUSY,   // 发送忙
    H_HOSTCAN_ERROR   // 驱动异常
} H_HostCan_Result;

typedef struct
{
    uint32_t id;     // 扩展ID
    uint8_t data[8]; // 原协议负载
} H_HostCan_Frame;

typedef struct
{
    H_HostCan_Frame receive[H_HOSTCAN_RX_CAPACITY]; // 中断接收队列
    can_frame_t transmit;                           // 发送完成前保持有效
    volatile uint32_t read_index;                   // 队列读索引
    volatile uint32_t write_index;                  // 队列写索引
    volatile uint32_t receive_count;                // 队列占用数
    volatile uint32_t transmit_busy;                // 发送进行中
    volatile uint32_t needs_recovery;               // 异常恢复标志
    volatile uint32_t dropped_frames;               // 无效或溢出帧计数
    volatile uint32_t error_events;                 // 错误事件累计位
    uint32_t initialized;                           // 初始化状态
} H_HostCan_Context;

/*
 * 说明：初始化上位机侧CAN硬件驱动
 * 输入：p_context 零初始化且具有任务生命周期的硬件上下文
 * 输出：H_HostCan_Result 初始化结果
 */
H_HostCan_Result H_HostCan_Initialize(H_HostCan_Context *p_context);

/*
 * 说明：从中断队列取出一帧
 * 输入：p_context 上下文，p_frame 输出帧
 * 输出：H_HostCan_Result 接收结果
 */
H_HostCan_Result H_HostCan_Receive(H_HostCan_Context *p_context, H_HostCan_Frame *p_frame);
/*
 * 说明：启动单帧异步发送
 * 输入：p_context 上下文，p_frame 待发送帧
 * 输出：H_HostCan_Result 启动结果
 */
H_HostCan_Result H_HostCan_Send(H_HostCan_Context *p_context, const H_HostCan_Frame *p_frame);
/*
 * 说明：读取发送状态
 * 输入：p_context 上下文
 * 输出：H_HostCan_Result 完成、忙或错误
 */
H_HostCan_Result H_HostCan_GetTransmitState(H_HostCan_Context *p_context);
/*
 * 说明：终止旧发送并重新打开CAN0
 * 输入：p_context 上下文
 * 输出：H_HostCan_Result 恢复结果
 */
H_HostCan_Result H_HostCan_Recover(H_HostCan_Context *p_context);

/*
 * 说明：处理上位机侧CAN接收、发送及错误事件
 * 输入：p_args FSP CAN回调参数
 * 输出：无
 */
void H_HostCan_Callback(can_callback_args_t *p_args);

#endif /* CAN_H_HOST_CAN_H_ */
