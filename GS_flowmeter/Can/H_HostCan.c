/*
 * H_HostCan.c
 *
 * Created on: 2026年9月12日
 * Author: YXZ
 */

#include "H_HostCan.h"

#include "HostCanStack.h"
#include <string.h>

/*
 * 说明：初始化上位机侧CAN硬件驱动
 * 输入：p_context 硬件上下文
 * 输出：H_HostCan_Result 初始化结果
 */
H_HostCan_Result H_HostCan_Initialize(H_HostCan_Context *p_context)
{
    fsp_err_t error = FSP_SUCCESS; // 驱动操作结果
    if (NULL == p_context) { return H_HOSTCAN_ERROR; }
    if (0U != p_context->initialized) { return H_HOSTCAN_OK; }
    p_context->read_index = 0U;
    p_context->write_index = 0U;
    p_context->receive_count = 0U;
    p_context->transmit_busy = 0U;
    p_context->needs_recovery = 0U;
    error = g_can0.p_api->open(g_can0.p_ctrl, g_can0.p_cfg);
    if (FSP_SUCCESS != error) { return H_HOSTCAN_ERROR; }
    error = g_can0.p_api->callbackSet(g_can0.p_ctrl, H_HostCan_Callback, p_context, NULL);
    if (FSP_SUCCESS != error)
    {
        (void) g_can0.p_api->close(g_can0.p_ctrl);
        return H_HOSTCAN_ERROR;
    }
    p_context->initialized = 1U;
    return H_HOSTCAN_OK;
}

/*
 * 说明：在临界区内从中断队列取出完整帧
 * 输入：p_context 上下文，p_frame 输出帧
 * 输出：H_HostCan_Result 接收结果
 */
H_HostCan_Result H_HostCan_Receive(H_HostCan_Context *p_context, H_HostCan_Frame *p_frame)
{
    H_HostCan_Result result = H_HOSTCAN_EMPTY; // 接收结果
    if ((NULL == p_context) || (NULL == p_frame) || (0U == p_context->initialized))
    { return H_HOSTCAN_ERROR; }
    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;
    if (0U != p_context->receive_count)
    {
        *p_frame = p_context->receive[p_context->read_index];
        p_context->read_index = (p_context->read_index + 1U) % H_HOSTCAN_RX_CAPACITY;
        p_context->receive_count--;
        result = H_HOSTCAN_OK;
    }
    FSP_CRITICAL_SECTION_EXIT;
    return result;
}

/*
 * 说明：启动非阻塞发送，帧缓冲保持到完成中断
 * 输入：p_context 上下文，p_frame 输入帧
 * 输出：H_HostCan_Result 启动结果
 */
H_HostCan_Result H_HostCan_Send(H_HostCan_Context *p_context, const H_HostCan_Frame *p_frame)
{
    fsp_err_t error = FSP_SUCCESS; // 驱动操作结果
    H_HostCan_Result state = H_HostCan_GetTransmitState(p_context); // 发送状态
    if ((NULL == p_frame) || (p_frame->id > 0x1FFFFFFFUL)) { return H_HOSTCAN_ERROR; }
    if (H_HOSTCAN_OK != state) { return state; }
    memset(&p_context->transmit, 0, sizeof(p_context->transmit));
    p_context->transmit.id = p_frame->id;
    p_context->transmit.id_mode = CAN_ID_MODE_EXTENDED;
    p_context->transmit.type = CAN_FRAME_TYPE_DATA;
    p_context->transmit.data_length_code = 8U;
    memcpy(p_context->transmit.data, p_frame->data, 8U);
    p_context->transmit_busy = 1U;
    error = g_can0.p_api->write(g_can0.p_ctrl, H_HOSTCAN_TX_MAILBOX, &p_context->transmit);
    if (FSP_SUCCESS != error)
    {
        p_context->transmit_busy = 0U;
        p_context->needs_recovery = 1U;
        return H_HOSTCAN_ERROR;
    }
    return H_HOSTCAN_OK;
}

/*
 * 说明：查询发送及异常状态
 * 输入：p_context 上下文
 * 输出：H_HostCan_Result 当前状态
 */
H_HostCan_Result H_HostCan_GetTransmitState(H_HostCan_Context *p_context)
{
    if ((NULL == p_context) || (0U == p_context->initialized) || (0U != p_context->needs_recovery))
    { return H_HOSTCAN_ERROR; }
    return (0U != p_context->transmit_busy) ? H_HOSTCAN_BUSY : H_HOSTCAN_OK;
}

/*
 * 说明：关闭后重新打开，终止旧请求并保留统计计数
 * 输入：p_context 上下文
 * 输出：H_HostCan_Result 恢复结果
 */
H_HostCan_Result H_HostCan_Recover(H_HostCan_Context *p_context)
{
    if (NULL == p_context) { return H_HOSTCAN_ERROR; }
    if (0U != p_context->initialized)
    {
        if (FSP_SUCCESS != g_can0.p_api->close(g_can0.p_ctrl)) { return H_HOSTCAN_ERROR; }
        p_context->initialized = 0U;
    }
    return H_HostCan_Initialize(p_context);
}

/*
 * 说明：处理上位机侧CAN接收、发送及错误事件
 * 输入：p_args FSP CAN回调参数
 * 输出：无
 */
void H_HostCan_Callback(can_callback_args_t *p_args)
{
    H_HostCan_Context *p_context = NULL; // 由FSP绑定的硬件上下文
    if ((NULL == p_args) || (NULL == p_args->p_context)) { return; }
    p_context = (H_HostCan_Context *) p_args->p_context;
    if (CAN_EVENT_RX_COMPLETE == p_args->event)
    {
        if ((CAN_ID_MODE_EXTENDED != p_args->frame.id_mode) ||
            (CAN_FRAME_TYPE_DATA != p_args->frame.type) || (8U != p_args->frame.data_length_code) ||
            (p_args->frame.id > 0x1FFFFFFFUL) || (p_context->receive_count >= H_HOSTCAN_RX_CAPACITY))
        {
            p_context->dropped_frames++;
            return;
        }
        p_context->receive[p_context->write_index].id = p_args->frame.id;
        memcpy(p_context->receive[p_context->write_index].data, p_args->frame.data, 8U);
        p_context->write_index = (p_context->write_index + 1U) % H_HOSTCAN_RX_CAPACITY;
        p_context->receive_count++;
    }
    else if ((CAN_EVENT_TX_COMPLETE == p_args->event) && (H_HOSTCAN_TX_MAILBOX == p_args->mailbox))
    {
        p_context->transmit_busy = 0U;
    }
    else
    {
        p_context->error_events |= (uint32_t) p_args->event;
        if (CAN_EVENT_MAILBOX_MESSAGE_LOST == p_args->event) { p_context->dropped_frames++; }
        if (0U != ((uint32_t) p_args->event &
                   (CAN_EVENT_ERR_BUS_OFF | CAN_EVENT_TX_ABORTED | CAN_EVENT_ERR_BUS_LOCK)))
        {
            p_context->needs_recovery = 1U;
        }
    }
}
