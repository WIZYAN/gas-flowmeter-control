/*
 * Created on: 2026年9月20日
 * Author: CI
 */
#ifndef MFC_H_MFC_CAN_H_
#define MFC_H_MFC_CAN_H_
#include <stdint.h>
#include "FreeRTOS.h"
#include "r_spi_api.h"

#define H_MFCCAN_BITRATE (250000UL) // 用户确定的下行CAN速率
#define H_MFCCAN_OSCILLATOR (8000000UL) // 原理图Y1为8MHz，实装待核对
#define H_MFCCAN_RX_CAPACITY (4U) // MfcTask内部收帧缓冲，不是跨任务队列
#define H_MFCCAN_TX_TIMEOUT_MS (50U) // 单帧发送完成期限

typedef enum
{
    H_MFCCAN_OK = 0, // 完成
    H_MFCCAN_EMPTY, // 没有收到完整帧
    H_MFCCAN_BUSY, // 发送进行中
    H_MFCCAN_ERROR // SPI、CAN或缓冲异常，必须恢复
} H_MfcCan_Result;

typedef struct
{
    uint32_t id; // 29位扩展帧标识符
    uint8_t data[8]; // CAN_USER固定8字节
} H_MfcCan_Frame;

typedef struct
{
    H_MfcCan_Frame receive[H_MFCCAN_RX_CAPACITY]; // 已取出的有效扩展数据帧
    H_MfcCan_Frame transmit; // 单笔待发送帧
    uint8_t spi_tx[16]; // 一次片选内完整SPI指令
    uint8_t spi_rx[16]; // SPI异步回调前必须保持有效
    volatile uint32_t spi_done; // SPI回调完成标记
    volatile uint32_t spi_error; // SPI回调错误标记
    uint32_t spi_open; // FSP SPI已打开
    uint32_t initialized; // MCP2515处于正常模式
    uint32_t fault; // 锁存异常，禁止继续发送
    uint32_t transmit_state; // 0空闲，1软件待发，2等待总线发送完成
    uint32_t read_index; // 本任务收帧索引
    uint32_t receive_count; // 本任务缓冲占用
    uint32_t rejected_frames; // 非扩展、远程或非8字节帧累计数
    TickType_t transmit_tick; // 总线发送起始节拍
} H_MfcCan_Context;

/*
 * 说明：在UART事务已停止后初始化MCP2515，配置8MHz/250k及扩展帧过滤
 * 输入：p_context 长期有效的零初始化状态，filter_id/filter_mask 29位接收过滤条件
 * 输出：H_MfcCan_Result 初始化结果；仅任务上下文、开中断调用
 */
H_MfcCan_Result H_MfcCan_Initialize(H_MfcCan_Context *p_context, uint32_t filter_id, uint32_t filter_mask);
/*
 * 说明：轮询两个硬件接收缓冲并推进一次发送；INT未连接，不依赖外部中断
 * 输入：p_context 本任务硬件状态，now 当前1ms节拍
 * 输出：无；故障锁存在fault中
 */
void H_MfcCan_Process(H_MfcCan_Context *p_context, TickType_t now);
/*
 * 说明：只复制一帧到待发槽，不访问SPI；实际发送由Process执行
 * 输入：p_context 状态，p_frame 29位ID及8字节负载
 * 输出：H_MfcCan_Result 接收、忙或故障
 */
H_MfcCan_Result H_MfcCan_Send(H_MfcCan_Context *p_context, const H_MfcCan_Frame *p_frame);
/*
 * 说明：读取发送状态，不自动重发，OK仅代表CAN总线发送完成
 * 输入：p_context 状态
 * 输出：H_MfcCan_Result 状态
 */
H_MfcCan_Result H_MfcCan_GetTransmitState(const H_MfcCan_Context *p_context);
/*
 * 说明：取出本任务缓冲中的一帧
 * 输入：p_context 状态，p_frame 输出帧
 * 输出：H_MfcCan_Result 状态
 */
H_MfcCan_Result H_MfcCan_Receive(H_MfcCan_Context *p_context, H_MfcCan_Frame *p_frame);
/*
 * 说明：用SPI复位MCP2515终止待发并清空旧帧；成功后才能切换RS485
 * 输入：p_context 状态；不可在临界区或ISR调用
 * 输出：H_MfcCan_Result 停止结果，失败时不能假定旧CAN发送已停止
 */
H_MfcCan_Result H_MfcCan_Stop(H_MfcCan_Context *p_context);
/*
 * 说明：记录SPI完成事件；不解析CAN，不发送任务间业务消息
 * 输入：p_args FSP回调参数
 * 输出：无
 */
void H_MFC_CAN_SpiCallback(spi_callback_args_t *p_args);
#endif
