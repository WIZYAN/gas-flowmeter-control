/*
 * H_EX201.h
 *
 * Created on: 2026年9月14日
 * Author: YXZ
 */

#ifndef MFC_H_EX201_H_
#define MFC_H_EX201_H_

#include <stddef.h>
#include <stdint.h>

#define H_EX201_MAX_TX_LENGTH    (48U) // RS485异步发送缓冲区长度
#define H_EX201_RX_BUFFER_LENGTH (64U) // SCI2中断接收环形缓冲区长度

typedef enum
{
    H_EX201_RESULT_OK = 0,             // 操作成功
    H_EX201_RESULT_INVALID_ARGUMENT,   // 输入参数无效
    H_EX201_RESULT_NOT_INITIALIZED,    // 硬件模块尚未初始化
    H_EX201_RESULT_BUSY,               // 上一次发送尚未完成
    H_EX201_RESULT_NO_DATA,            // 暂时没有可读取字节
    H_EX201_RESULT_RECEIVE_OVERFLOW,   // 接收环形缓冲区溢出
    H_EX201_RESULT_UART_ERROR,         // UART发生奇偶、帧或硬件溢出错误
    H_EX201_RESULT_DRIVER_ERROR        // FSP驱动调用失败
} H_EX201_Result;

typedef struct
{
    uint8_t transmit_buffer[H_EX201_MAX_TX_LENGTH];       // 异步发送期间持续有效的发送缓冲区
    volatile uint8_t receive_buffer[H_EX201_RX_BUFFER_LENGTH]; // UART中断写入的接收环形缓冲区
    volatile uint32_t receive_read_index;                 // 接收环形缓冲区读取位置
    volatile uint32_t receive_write_index;                // 接收环形缓冲区写入位置
    volatile uint32_t receive_count;                      // 接收环形缓冲区已有字节数
    volatile uint32_t initialized;                        // 硬件模块初始化状态
    volatile uint32_t transmit_busy;                      // UART发送忙状态
    volatile uint32_t receive_overflow;                   // 接收环形缓冲区溢出状态
    volatile uint32_t uart_error;                         // UART接收错误状态
} H_EX201_Context;

/*
 * 说明：初始化流量计RS485硬件模块并绑定回调上下文
 * 输入：p_context 硬件模块上下文
 * 输出：H_EX201_Result 初始化结果
 */
H_EX201_Result H_EX201_Initialize(H_EX201_Context *p_context);

/*
 * 说明：通过RS485异步发送数据
 * 输入：p_context  硬件模块上下文
 *      p_data      待发送数据
 *      data_length 待发送数据长度
 * 输出：H_EX201_Result 发送启动结果
 */
H_EX201_Result H_EX201_Send(
    H_EX201_Context *p_context,
    const uint8_t *p_data,
    size_t data_length);

/*
 * 说明：从SCI2接收环形缓冲区读取一个字节
 * 输入：p_context 硬件模块上下文
 *      p_data     输出字节
 * 输出：H_EX201_Result 读取结果
 */
H_EX201_Result H_EX201_ReceiveByte(
    H_EX201_Context *p_context,
    uint8_t *p_data);

/*
 * 说明：中止异常发送并将RS485恢复到接收状态
 * 输入：p_context 硬件模块上下文
 * 输出：H_EX201_Result 恢复结果
 */
H_EX201_Result H_EX201_Recover(H_EX201_Context *p_context);

/*
 * 说明：查询RS485是否仍在发送数据
 * 输入：p_context 硬件模块上下文
 * 输出：uint32_t 非0表示正在发送，0表示发送空闲
 */
uint32_t H_EX201_IsTransmitBusy(H_EX201_Context *p_context);

/*
 * 说明：清空SCI2接收环形缓冲区和错误状态
 * 输入：p_context 硬件模块上下文
 * 输出：无
 */
void H_EX201_ResetReceiver(H_EX201_Context *p_context);

#endif /* MFC_H_EX201_H_ */
