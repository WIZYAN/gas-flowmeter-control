/* Created on: 2026年9月20日，Author: CI */
#include "A_MfcCan.h"
#include <math.h>
#include <float.h>

/*
 * 说明：打开MCP2515并清空本事务
 * 输入：p_context 状态
 * 输出：A_EX201_Result 结果
 */
A_EX201_Result A_MfcCan_Initialize(A_MfcCan_Context *p_context)
{
    if (NULL == p_context || NULL == p_context->p_transport) { return A_EX201_RESULT_INVALID_ARGUMENT; }
    if (H_MFCCAN_OK != F_MfcCan_Initialize(p_context->p_transport, A_MFCCAN_LOCAL_ADDRESS))
    {
        return A_EX201_RESULT_INITIALIZE_ERROR;
    }
    p_context->initialized = 1U;
    p_context->state = 0U;
    return A_EX201_RESULT_OK;
}
/*
 * 说明：中止CAN，旧结果不得跨链路保留
 * 输入：p_context 状态
 * 输出：A_EX201_Result 结果
 */
A_EX201_Result A_MfcCan_Stop(A_MfcCan_Context *p_context)
{
    if (NULL == p_context || NULL == p_context->p_transport) { return A_EX201_RESULT_INVALID_ARGUMENT; }
    if (H_MFCCAN_OK != F_MfcCan_Stop(p_context->p_transport)) { return A_EX201_RESULT_RECOVERY_ERROR; }
    p_context->initialized = 0U;
    p_context->state = 0U;
    return A_EX201_RESULT_OK;
}
/*
 * 说明：构造与原CAN_USER完全一致的单参数请求
 * 输入：p_context 状态，node 节点，parameter 参数，write 读写选择，value 位模式，now 节拍
 * 输出：A_EX201_Result 结果
 */
A_EX201_Result A_MfcCan_Start(A_MfcCan_Context *p_context, uint16_t node, uint16_t parameter,
                             uint32_t write, uint32_t value, TickType_t now)
{
    F_CanUser_Message g_message = {0}; // 逻辑请求
    F_CanUser_Frame g_frame = {0}; // 编码请求
    if (NULL == p_context || node < 1U || node > 6U || (write && parameter != A_MFCCAN_TARGET))
    { return A_EX201_RESULT_INVALID_ARGUMENT; }
    if (!p_context->initialized) { return A_EX201_RESULT_NOT_INITIALIZED; }
    if (p_context->state != 0U) { return A_EX201_RESULT_BUSY; }
    g_message.function = (uint8_t) (write ? F_CANUSER_WRITE : F_CANUSER_READ);
    g_message.target_type = Type_FLOW;
    g_message.target_address = (uint8_t) node;
    g_message.source_type = Type_Header;
    g_message.source_address = A_MFCCAN_LOCAL_ADDRESS;
    g_message.address = parameter;
    g_message.count = 1U;
    g_message.value = value;
    if (F_CANUSER_RESULT_OK != F_CanUser_Encode(&g_message, &g_frame) ||
        H_MFCCAN_OK != F_MfcCan_Send(p_context->p_transport, &g_frame)) { return A_EX201_RESULT_SEND_ERROR; }
    p_context->parameter = parameter;
    p_context->node = (uint8_t) node;
    p_context->function = g_message.function;
    p_context->state = 1U;
    p_context->started_tick = now;
    p_context->result = A_EX201_RESULT_BUSY;
    p_context->device_error = 0U;
    return A_EX201_RESULT_OK;
}
/*
 * 说明：处理最多四个缓存帧，先判断期限，再允许硬件推进，过期写不会才开始发送
 * 输入：p_context 状态，now 节拍
 * 输出：无
 */
void A_MfcCan_Process(A_MfcCan_Context *p_context, TickType_t now)
{
    F_CanUser_Frame g_frame = {0}; // 接收帧
    F_CanUser_Message g_message = {0}; // 接收逻辑字段
    F_MfcCan_Result state = H_MFCCAN_OK; // 驱动状态
    uint32_t index = 0U; // 本轮帧数上限
    uint32_t expected = 0U; // 预期回复功能码
    if (NULL == p_context || !p_context->initialized) { return; }
    if (p_context->state == 1U && (TickType_t) (now - p_context->started_tick) >= A_MFCCAN_RESPONSE_MS)
    {
        p_context->result = A_EX201_RESULT_RESPONSE_TIMEOUT;
        p_context->state = 2U;
        return; // 调度层取走错误后立即Stop，清除尚未发出的待发槽。
    }
    F_MfcCan_Process(p_context->p_transport, now);
    state = F_MfcCan_GetTransmitState(p_context->p_transport);
    if (state == H_MFCCAN_ERROR)
    {
        if (p_context->state == 1U) { p_context->result = A_EX201_RESULT_RECEIVE_ERROR; p_context->state = 2U; }
        return;
    }
    expected = p_context->function == F_CANUSER_READ ? F_CANUSER_READ_RETURN : F_CANUSER_WRITE_RETURN;
    for (index = 0U; index < H_MFCCAN_RX_CAPACITY; index++)
    {
        if (H_MFCCAN_OK != F_MfcCan_Receive(p_context->p_transport, &g_frame)) { break; }
        if (p_context->state != 1U || state != H_MFCCAN_OK ||
            F_CANUSER_RESULT_OK != F_CanUser_Decode(&g_frame, &g_message) ||
            g_message.source_type != Type_FLOW || g_message.source_address != p_context->node ||
            g_message.target_type != Type_Header || g_message.target_address != A_MFCCAN_LOCAL_ADDRESS ||
            g_message.function != expected || g_message.address != p_context->parameter || g_message.count != 1U)
        {
            p_context->rejected_frames++;
            continue;
        }
        p_context->value = g_message.value;
        p_context->result = A_EX201_RESULT_OK;
        if (p_context->function == F_CANUSER_WRITE && g_message.value != 0U)
        {
            p_context->device_error = g_message.value;
            p_context->result = g_message.value <= 255U ? A_EX201_RESULT_DEVICE_NG : A_EX201_RESULT_PROTOCOL_ERROR;
        }
        p_context->state = 2U;
    }
}
/*
 * 说明：消费单笔结果，未完成不能更改输出
 * 输入：p_context 状态，p_value 输出
 * 输出：A_EX201_Result 结果
 */
A_EX201_Result A_MfcCan_GetResult(A_MfcCan_Context *p_context, uint32_t *p_value)
{
    if (NULL == p_context) { return A_EX201_RESULT_INVALID_ARGUMENT; }
    if (p_context->state == 1U) { return A_EX201_RESULT_BUSY; }
    if (p_context->state != 2U) { return A_EX201_RESULT_NO_RESULT; }
    p_context->state = 0U;
    if (p_value != NULL && p_context->result == A_EX201_RESULT_OK) { *p_value = p_context->value; }
    return p_context->result;
}
/*
 * 说明：逐字段检查枚举及范围；标识和版本必须精确匹配
 * 输入：p_context 状态，p_info 通道参数
 * 输出：A_EX201_Result 结果
 */
A_EX201_Result A_MfcCan_GetInfo(A_MfcCan_Context *p_context, A_EX201_DeviceInfo *p_info)
{
    uint32_t value = 0U; // 原始uint32数据
    A_EX201_Result result = A_EX201_RESULT_INVALID_ARGUMENT; // 结果
    if (NULL == p_context || NULL == p_info) { return result; }
    result = A_MfcCan_GetResult(p_context, &value);
    if (result != A_EX201_RESULT_OK) { return result; }
    switch (p_context->parameter)
    {
        case A_MFCCAN_DEVICE_ID:
            return value == A_MFCCAN_IDENTITY ? A_EX201_RESULT_OK : A_EX201_RESULT_RESPONSE_MISMATCH;
        case A_MFCCAN_VERSION:
            return value == A_MFCCAN_PROFILE_VERSION ? A_EX201_RESULT_OK : A_EX201_RESULT_RESPONSE_MISMATCH;
        case A_MFCCAN_FULL_SCALE:
            if (value < 1U || value > 9999U) { break; }
            p_info->full_scale_mantissa = (uint16_t) value;
            p_info->valid_flags |= A_EX201_DEVICE_INFO_FULL_SCALE_VALID;
            return A_EX201_RESULT_OK;
        case A_MFCCAN_DECIMAL:
            if (value > 3U) { break; }
            p_info->decimal_places = (uint8_t) value;
            p_info->valid_flags |= A_EX201_DEVICE_INFO_DECIMAL_VALID;
            return A_EX201_RESULT_OK;
        case A_MFCCAN_UNIT:
            if (value > 1U) { break; }
            p_info->flow_unit = (A_EX201_FlowUnit) value;
            p_info->valid_flags |= A_EX201_DEVICE_INFO_UNIT_VALID;
            return A_EX201_RESULT_OK;
        case A_MFCCAN_SOURCE:
            if (value > 1U) { break; }
            p_info->flow_source = (A_EX201_FlowSource) value;
            p_info->valid_flags |= A_EX201_DEVICE_INFO_SOURCE_VALID;
            return A_EX201_RESULT_OK;
        case A_MFCCAN_SETTING:
            if (value > 2U) { break; }
            p_info->valve_setting = (A_EX201_ValveSetting) value;
            p_info->valid_flags |= A_EX201_DEVICE_INFO_VALVE_SETTING_VALID;
            return A_EX201_RESULT_OK;
        case A_MFCCAN_VALVE:
            if (value > 3U) { break; }
            p_info->valve_state = (A_EX201_ValveState) value;
            p_info->valid_flags |= A_EX201_DEVICE_INFO_VALVE_STATE_VALID;
            return A_EX201_RESULT_OK;
        case A_MFCCAN_ALARM:
            if (value > 7U) { break; }
            p_info->alarm_state = (uint8_t) value;
            p_info->valid_flags |= A_EX201_DEVICE_INFO_ALARM_VALID;
            return A_EX201_RESULT_OK;
        default:
            break;
    }
    return A_EX201_RESULT_PROTOCOL_ERROR;
}
/*
 * 说明：浮点流量按已确认的小数位换算；拒绝NaN、无穷和无法按分辨率表示的值
 * 输入：p_context 状态，p_info 参数，p_mantissa 输出
 * 输出：A_EX201_Result 结果
 */
A_EX201_Result A_MfcCan_GetFlow(A_MfcCan_Context *p_context, const A_EX201_DeviceInfo *p_info, int32_t *p_mantissa)
{
    static const float s_scales[4] = {1.0F, 10.0F, 100.0F, 1000.0F}; // 十进制比例
    uint32_t bits = 0U; // float位模式
    float value = 0.0F, scaled = 0.0F; // 工程值与尾数
    int32_t rounded = 0; // 最近整数
    A_EX201_Result result = A_EX201_RESULT_INVALID_ARGUMENT; // 结果
    if (NULL == p_context || NULL == p_info || NULL == p_mantissa) { return result; }
    result = A_MfcCan_GetResult(p_context, &bits);
    if (result != A_EX201_RESULT_OK) { return result; }
    if (p_info->decimal_places > 3U || !(p_info->valid_flags & A_EX201_DEVICE_INFO_DECIMAL_VALID))
    { return A_EX201_RESULT_PROTOCOL_ERROR; }
    value = F_CanUser_BitsToFloat(bits);
    scaled = value * s_scales[p_info->decimal_places];
    if (!isfinite(scaled) || scaled < -9999.0F || scaled > 9999.0F ||
        (p_context->parameter == A_MFCCAN_CONFIRMED && scaled < 0.0F)) { return A_EX201_RESULT_PROTOCOL_ERROR; }
    rounded = (int32_t) (scaled + (scaled < 0.0F ? -0.5F : 0.5F));
    if (fabsf(scaled - (float) rounded) > 2.0F * FLT_EPSILON * fmaxf(1.0F, fabsf(scaled)))
    { return A_EX201_RESULT_PROTOCOL_ERROR; }
    *p_mantissa = rounded;
    return A_EX201_RESULT_OK;
}
