/* Created on: 2026年9月20日，Author: CI */
#include "A_MfcLink.h"

/*
 * 说明：旧链路数据立即失效；新链路必须重新采集六路参数
 * 输入：p_context 状态
 * 输出：无
 */
static void A_MfcLink_ClearChannels(A_MFC_Context *p_context)
{
    uint32_t index = 0U; // 通道索引
    for (index = 0U; index < A_MFC_CHANNEL_COUNT; index++)
    {
        A_MFC_Channel *p_channel = &p_context->p_channels[index]; // 当前通道
        p_channel->online = 0U;
        p_channel->flow_valid = 0U;
        p_channel->device_info.valid_flags = 0U;
        p_channel->initialize_state = A_MFC_INIT_RUNNING;
        p_channel->initialize_step = 0U;
        p_channel->status_step = 0U;
        p_channel->prefer_status = 0U;
        p_channel->consecutive_failures = 0U;
        p_channel->can_profile_checked = 0U;
        p_channel->revision++;
    }
    p_context->active = 0U;
    p_context->next_channel = 0U;
    p_context->guard_active = 0U;
    p_context->valid_responses = 0U;
}

/*
 * 说明：每个节点先探测485再探测CAN，失败后换下一个节点，避免只识别地址1
 * 输入：p_context 状态，now 当前节拍
 * 输出：无
 */
static void A_MfcLink_NextProbe(A_MFC_Context *p_context, TickType_t now)
{
    if (p_context->probe_link == A_MFC_LINK_RS485)
    {
        p_context->probe_link = A_MFC_LINK_CAN;
    }
    else
    {
        p_context->probe_link = A_MFC_LINK_RS485;
        p_context->probe_channel = (p_context->probe_channel + 1U) % A_MFC_CHANNEL_COUNT;
    }
    p_context->probe_step = 0U;
    p_context->probe_wait = 1U;
    p_context->probe_failed = 1U;
    p_context->probe_tick = now;
}

/*
 * 说明：确认停止两个旧后端后才初始化本次后端，避免共享EN引脚相互改写
 * 输入：p_context 状态
 * 输出：A_EX201_Result 结果；停止失败时保持当前探测方向，不启动另一后端
 */
static A_EX201_Result A_MfcLink_Prepare(A_MFC_Context *p_context)
{
    A_EX201_Result result = A_MfcCan_Stop(p_context->p_can); // 首先停止CAN发送
    if (result != A_EX201_RESULT_OK) { return A_EX201_RESULT_RECOVERY_ERROR; }
    if (p_context->p_transaction->initialized)
    {
        result = A_EX201_Recover(p_context->p_transaction);
        if (result != A_EX201_RESULT_OK) { return A_EX201_RESULT_RECOVERY_ERROR; }
    }
    if (p_context->probe_link == A_MFC_LINK_CAN) { return A_MfcCan_Initialize(p_context->p_can); }
    return p_context->p_transaction->initialized ? A_EX201_RESULT_OK : A_EX201_Initialize(p_context->p_transaction);
}

/*
 * 说明：用两种不同只读参数确认同一节点，不把重复帧或无关流量当作两次确认
 * 输入：p_context 状态，now 当前节拍
 * 输出：uint32_t 非0表示锁定成功
 */
uint32_t A_MfcLink_Process(A_MFC_Context *p_context, TickType_t now)
{
    A_EX201_Result result = A_EX201_RESULT_OK; // 当前探测结果
    A_EX201_DeviceInfo g_info = {0}; // 探测专用临时参数，不发布成通道在线数据
    uint16_t address = 0U; // EX201地址
    uint32_t second = 0U; // 正在读取第二个参数
    if (NULL == p_context || !p_context->configured || NULL == p_context->p_channels ||
        NULL == p_context->p_transaction || NULL == p_context->p_can ||
        p_context->probe_channel >= A_MFC_CHANNEL_COUNT) { return 0U; }
    address = p_context->p_channels[p_context->probe_channel].address;
    second = p_context->probe_step >= 2U;
    if (p_context->selected_link != A_MFC_LINK_PROBING)
    {
        if (p_context->transport_ready &&
            (TickType_t) (now - p_context->last_valid_tick) < A_MFC_LINK_LOSS_MS) { return 1U; }
        // 只有整条链路失去有效响应才解除锁定，不因单路掉线切换。
        p_context->selected_link = A_MFC_LINK_PROBING;
        p_context->transport_ready = 0U;
        p_context->probe_link = A_MFC_LINK_RS485;
        p_context->probe_channel = 0U;
        p_context->probe_step = 0U;
        p_context->probe_wait = 1U;
        p_context->probe_tick = now;
        A_MfcLink_ClearChannels(p_context);
    }
    if (p_context->probe_wait)
    {
        if ((TickType_t) (now - p_context->probe_tick) < A_MFCCAN_QUIET_MS) { return 0U; }
        p_context->probe_wait = 0U;
    }
    if (p_context->probe_link == 0U) { p_context->probe_link = A_MFC_LINK_RS485; }
    if (p_context->probe_step == 0U)
    {
        result = A_MfcLink_Prepare(p_context);
        if (result == A_EX201_RESULT_RECOVERY_ERROR)
        {
            p_context->probe_wait = 1U;
            p_context->probe_failed = 1U;
            p_context->probe_tick = now;
            return 0U; // 旧事务停止未确认，不能盲目切换。
        }
        if (result != A_EX201_RESULT_OK) { A_MfcLink_NextProbe(p_context, now); return 0U; }
    }
    if (p_context->probe_step == 0U || p_context->probe_step == 2U)
    {
        if (p_context->probe_link == A_MFC_LINK_CAN)
        {
            result = A_MfcCan_Start(p_context->p_can, (uint16_t) (p_context->probe_channel + 1U),
                second ? A_MFCCAN_VERSION : A_MFCCAN_DEVICE_ID, 0U, 0U, now);
        }
        else
        {
            result = second ? A_EX201_ReadDecimalPlaces(p_context->p_transaction, address, now) :
                              A_EX201_ReadFullScale(p_context->p_transaction, address, now);
        }
        if (result != A_EX201_RESULT_OK) { A_MfcLink_NextProbe(p_context, now); return 0U; }
        p_context->probe_step++;
        return 0U;
    }
    if (p_context->probe_link == A_MFC_LINK_CAN)
    {
        A_MfcCan_Process(p_context->p_can, now);
        result = A_MfcCan_GetInfo(p_context->p_can, &g_info);
    }
    else
    {
        A_EX201_Process(p_context->p_transaction, now);
        result = A_EX201_GetDeviceInfoResult(p_context->p_transaction, &g_info);
    }
    if (result == A_EX201_RESULT_BUSY) { return 0U; }
    if (result != A_EX201_RESULT_OK) { A_MfcLink_NextProbe(p_context, now); return 0U; }
    if (p_context->probe_step == 1U) { p_context->probe_step = 2U; return 0U; }
    p_context->selected_link = p_context->probe_link;
    p_context->transport_ready = 1U;
    p_context->transport_attempted = 0U;
    p_context->last_valid_tick = now;
    p_context->probe_failed = 0U;
    A_MfcLink_ClearChannels(p_context);
    p_context->valid_responses = 2U;
    return 1U;
}
