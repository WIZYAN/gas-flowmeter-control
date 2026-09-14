/*
 * H_EX201.c
 *
 * Created on: 2026年9月14日
 * Author: YXZ
 */

#include "H_EX201.h"

#include "MfcTask.h"

#if !defined(MFC_EN2)
#error "MFC RS485 direction pin is not defined."
#endif

#define H_EX201_DIRECTION_PIN MFC_EN2 // 流量计RS485收发方向控制引脚

#define H_EX201_TRANSMIT_LEVEL BSP_IO_LEVEL_LOW  // 按隔离电路设计意图设置发送，须经实板测量确认
#define H_EX201_RECEIVE_LEVEL  BSP_IO_LEVEL_HIGH // 按隔离电路设计意图设置接收，须经实板测量确认

#define H_EX201_UART_ERROR_EVENTS \
    (UART_EVENT_ERR_PARITY | UART_EVENT_ERR_FRAMING | UART_EVENT_ERR_OVERFLOW) // UART接收错误事件集合

/*
 * 说明：设置流量计RS485收发方向控制引脚电平
 * 输入：level 方向控制引脚电平
 * 输出：fsp_err_t FSP驱动调用结果
 */
static fsp_err_t H_EX201_SetDirection(bsp_io_level_t level)
{
    return g_ioport.p_api->pinWrite(
        g_ioport.p_ctrl,
        H_EX201_DIRECTION_PIN,
        level);
}

/*
 * 说明：复位SCI2接收环形缓冲区，调用者必须处于临界区或中断上下文
 * 输入：p_context 硬件模块上下文
 * 输出：无
 */
static void H_EX201_ResetReceiverInternal(H_EX201_Context *p_context)
{
    p_context->receive_read_index = 0U;
    p_context->receive_write_index = 0U;
    p_context->receive_count = 0U;
}

/*
 * 说明：初始化流量计RS485硬件模块并绑定回调上下文
 * 输入：p_context 硬件模块上下文
 * 输出：H_EX201_Result 初始化结果
 */
H_EX201_Result H_EX201_Initialize(H_EX201_Context *p_context)
{
    fsp_err_t error = FSP_SUCCESS; // FSP驱动调用结果

    if (p_context == NULL)
    {
        return H_EX201_RESULT_INVALID_ARGUMENT;
    }

    if (0U != p_context->initialized)
    {
        return H_EX201_RESULT_OK;
    }

    H_EX201_ResetReceiverInternal(p_context);
    p_context->transmit_busy = 0U;
    p_context->receive_overflow = 0U;
    p_context->uart_error = 0U;

    error = H_EX201_SetDirection(H_EX201_RECEIVE_LEVEL);

    if (FSP_SUCCESS != error)
    {
        return H_EX201_RESULT_DRIVER_ERROR;
    }

    error = g_mfc_uart.p_api->open(
        g_mfc_uart.p_ctrl,
        g_mfc_uart.p_cfg);

    if (FSP_SUCCESS != error)
    {
        return H_EX201_RESULT_DRIVER_ERROR;
    }

    error = g_mfc_uart.p_api->callbackSet(
        g_mfc_uart.p_ctrl,
        H_EX201_UartCallback,
        p_context,
        NULL);

    if (FSP_SUCCESS != error)
    {
        (void) g_mfc_uart.p_api->close(g_mfc_uart.p_ctrl);
        return H_EX201_RESULT_DRIVER_ERROR;
    }

    p_context->initialized = 1U;

    return H_EX201_RESULT_OK;
}

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
    size_t data_length)
{
    fsp_err_t error = FSP_SUCCESS;        // FSP驱动调用结果
    size_t data_index = 0U;               // 发送数据索引
    H_EX201_Result result = H_EX201_RESULT_OK; // 函数处理结果

    if ((p_context == NULL) ||
        (p_data == NULL) ||
        (0U == data_length) ||
        (data_length > H_EX201_MAX_TX_LENGTH))
    {
        return H_EX201_RESULT_INVALID_ARGUMENT;
    }

    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;

    if (0U == p_context->initialized)
    {
        result = H_EX201_RESULT_NOT_INITIALIZED;
    }
    else if (0U != p_context->transmit_busy)
    {
        result = H_EX201_RESULT_BUSY;
    }
    else
    {
        H_EX201_ResetReceiverInternal(p_context);
        p_context->receive_overflow = 0U;
        p_context->uart_error = 0U;

        for (data_index = 0U; data_index < data_length; data_index++)
        {
            p_context->transmit_buffer[data_index] = p_data[data_index];
        }

        p_context->transmit_busy = 1U;
    }

    FSP_CRITICAL_SECTION_EXIT;

    if (H_EX201_RESULT_OK != result)
    {
        return result;
    }

    error = H_EX201_SetDirection(H_EX201_TRANSMIT_LEVEL);

    if (FSP_SUCCESS == error)
    {
        error = g_mfc_uart.p_api->write(
            g_mfc_uart.p_ctrl,
            p_context->transmit_buffer,
            (uint32_t) data_length);
    }

    if (FSP_SUCCESS != error)
    {
        (void) H_EX201_SetDirection(H_EX201_RECEIVE_LEVEL);

        FSP_CRITICAL_SECTION_ENTER;
        p_context->transmit_busy = 0U;
        FSP_CRITICAL_SECTION_EXIT;

        return H_EX201_RESULT_DRIVER_ERROR;
    }

    return H_EX201_RESULT_OK;
}

/*
 * 说明：从SCI2接收环形缓冲区读取一个字节
 * 输入：p_context 硬件模块上下文
 *      p_data     输出字节
 * 输出：H_EX201_Result 读取结果
 */
H_EX201_Result H_EX201_ReceiveByte(
    H_EX201_Context *p_context,
    uint8_t *p_data)
{
    H_EX201_Result result = H_EX201_RESULT_OK; // 函数处理结果

    if ((p_context == NULL) || (p_data == NULL))
    {
        return H_EX201_RESULT_INVALID_ARGUMENT;
    }

    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;

    if (0U == p_context->initialized)
    {
        result = H_EX201_RESULT_NOT_INITIALIZED;
    }
    else if (0U != p_context->uart_error)
    {
        p_context->uart_error = 0U;
        result = H_EX201_RESULT_UART_ERROR;
    }
    else if (0U != p_context->receive_overflow)
    {
        p_context->receive_overflow = 0U;
        result = H_EX201_RESULT_RECEIVE_OVERFLOW;
    }
    else if (0U == p_context->receive_count)
    {
        result = H_EX201_RESULT_NO_DATA;
    }
    else
    {
        *p_data = p_context->receive_buffer[p_context->receive_read_index];
        p_context->receive_read_index++;

        if (p_context->receive_read_index >= H_EX201_RX_BUFFER_LENGTH)
        {
            p_context->receive_read_index = 0U;
        }

        p_context->receive_count--;
    }

    FSP_CRITICAL_SECTION_EXIT;

    return result;
}

/*
 * 说明：中止异常发送并将RS485恢复到接收状态
 * 输入：p_context 硬件模块上下文
 * 输出：H_EX201_Result 恢复结果
 */
H_EX201_Result H_EX201_Recover(H_EX201_Context *p_context)
{
    fsp_err_t abort_error = FSP_SUCCESS;     // UART中止发送结果
    fsp_err_t direction_error = FSP_SUCCESS; // RS485方向恢复结果

    if (p_context == NULL)
    {
        return H_EX201_RESULT_INVALID_ARGUMENT;
    }

    if (0U == p_context->initialized)
    {
        return H_EX201_RESULT_NOT_INITIALIZED;
    }

    abort_error = g_mfc_uart.p_api->communicationAbort(
        g_mfc_uart.p_ctrl,
        UART_DIR_TX);

    direction_error = H_EX201_SetDirection(H_EX201_RECEIVE_LEVEL);

    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;
    p_context->transmit_busy = 0U;
    H_EX201_ResetReceiverInternal(p_context);
    p_context->receive_overflow = 0U;
    p_context->uart_error = 0U;
    FSP_CRITICAL_SECTION_EXIT;

    if ((FSP_SUCCESS != abort_error) ||
        (FSP_SUCCESS != direction_error))
    {
        return H_EX201_RESULT_DRIVER_ERROR;
    }

    return H_EX201_RESULT_OK;
}

/*
 * 说明：查询RS485是否仍在发送数据
 * 输入：p_context 硬件模块上下文
 * 输出：uint32_t 非0表示正在发送，0表示发送空闲
 */
uint32_t H_EX201_IsTransmitBusy(H_EX201_Context *p_context)
{
    uint32_t transmit_busy = 0U; // 当前发送忙状态

    if (p_context == NULL)
    {
        return 0U;
    }

    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;
    transmit_busy = p_context->transmit_busy;
    FSP_CRITICAL_SECTION_EXIT;

    return transmit_busy;
}

/*
 * 说明：清空SCI2接收环形缓冲区和错误状态
 * 输入：p_context 硬件模块上下文
 * 输出：无
 */
void H_EX201_ResetReceiver(H_EX201_Context *p_context)
{
    if (p_context == NULL)
    {
        return;
    }

    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;
    H_EX201_ResetReceiverInternal(p_context);
    p_context->receive_overflow = 0U;
    p_context->uart_error = 0U;
    FSP_CRITICAL_SECTION_EXIT;
}

/*
 * 说明：处理SCI2 UART接收、发送完成及通信错误事件
 * 输入：p_args FSP UART回调参数
 * 输出：无
 */
void H_EX201_UartCallback(uart_callback_args_t *p_args)
{
    H_EX201_Context *p_context = NULL; // 与SCI2回调绑定的硬件上下文
    fsp_err_t error = FSP_SUCCESS;     // 方向切换结果

    if ((p_args == NULL) || (p_args->p_context == NULL))
    {
        return;
    }

    p_context = (H_EX201_Context *) p_args->p_context;

    if (0U != (p_args->event & H_EX201_UART_ERROR_EVENTS))
    {
        H_EX201_ResetReceiverInternal(p_context);
        p_context->uart_error = 1U;
        return;
    }

    if (UART_EVENT_TX_COMPLETE == p_args->event)
    {
        error = H_EX201_SetDirection(H_EX201_RECEIVE_LEVEL);

        if (FSP_SUCCESS != error)
        {
            p_context->uart_error = 1U;
        }

        p_context->transmit_busy = 0U;
        return;
    }

    if ((UART_EVENT_RX_CHAR != p_args->event) ||
        (0U != p_context->transmit_busy))
    {
        return;
    }

    if (p_context->receive_count >= H_EX201_RX_BUFFER_LENGTH)
    {
        H_EX201_ResetReceiverInternal(p_context);
        p_context->receive_overflow = 1U;
        return;
    }

    p_context->receive_buffer[p_context->receive_write_index] = (uint8_t) p_args->data;
    p_context->receive_write_index++;

    if (p_context->receive_write_index >= H_EX201_RX_BUFFER_LENGTH)
    {
        p_context->receive_write_index = 0U;
    }

    p_context->receive_count++;
}
