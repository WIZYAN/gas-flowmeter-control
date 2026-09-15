/*
 * Created on: 2026年9月14日
 * Author: YXZ
 */
#include "A_HostCan.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stddef.h>
#include <string.h>
#include <math.h>

/*
 * 说明：检查参数是否在本项目已定义的地址表中
 * 输入：address 参数地址
 * 输出：uint32_t 非0表示地址已定义
 */
static uint32_t A_HostCan_AddressDefined(uint16_t address)
{
    static const uint16_t s_channel_groups[] = {
        0x0000U, 0x0010U, 0x0020U, 0x0200U, 0x0110U, 0x0120U, 0x0130U,
        0x0140U, 0x0150U, 0x0160U, 0x0170U, 0x0180U, 0x0190U
    }; // 每个组内连续6个通道地址
    uint32_t index = 0U; // 参数组索引
    if (((address >= 0x0100U) && (address <= 0x010EU) && (0x0107U != address)) ||
        ((address >= 0x0300U) && (address <= 0x0309U))) { return 1U; }
    for (index = 0U; index < (sizeof(s_channel_groups) / sizeof(s_channel_groups[0])); index++)
    {
        if (((uint32_t) address >= s_channel_groups[index]) &&
            ((uint32_t) address < ((uint32_t) s_channel_groups[index] + A_HOSTCAN_CHANNEL_COUNT)))
        { return 1U; }
    }
    return 0U;
}

/*
 * 说明：判断九阀完整目标是否满足V7/V9联动及与V8互斥
 * 输入：mask 九阀目标
 * 输出：uint32_t 非0合法
 */
static uint32_t A_HostCan_ValvesLegal(uint32_t mask)
{
    uint32_t group = mask & A_HOSTCAN_VALVE_GROUP79; // V7/V9组合
    return ((0U == (mask & ~A_HOSTCAN_VALVE_MASK)) &&
            ((0U == group) || (A_HOSTCAN_VALVE_GROUP79 == group)) &&
            !((0U != group) && (0U != (mask & A_HOSTCAN_VALVE8)))) ? 1U : 0U;
}

/*
 * 说明：记录可由上位机查询的最近错误，不修改协议功能码
 * 输入：p_context 上下文，p_request 原请求，address 出错地址，code 错误码
 * 输出：无
 */
static void A_HostCan_RecordError(A_HostCan_Context *p_context, const F_CanUser_Message *p_request,
                                 uint16_t address, uint32_t code)
{
    p_context->last_error_address = address;
    p_context->last_error_code = code;
    p_context->last_error_function = p_request->function;
}

/*
 * 说明：按原协议生成回复，参数地址保留，来源与目标互换
 * 输入：p_context 上下文，p_request 请求，function 回复功能，address 参数地址，value 数据
 * 输出：无；调用者保证队列有空位
 */
static void A_HostCan_QueueReply(A_HostCan_Context *p_context, const F_CanUser_Message *p_request,
                               uint8_t function, uint16_t address, uint32_t value)
{
    F_CanUser_Message g_reply = {0}; // 待编码回复
    uint32_t index = (p_context->transmit_read + p_context->transmit_count) % A_HOSTCAN_TX_CAPACITY; // 入队位置
    g_reply.function = function;
    g_reply.source_type = Type_FLOW;
    g_reply.source_address = p_context->self_address;
    g_reply.target_type = p_request->source_type;
    g_reply.target_address = p_request->source_address;
    g_reply.address = address;
    g_reply.count = 1U;
    g_reply.value = value;
    if ((p_context->transmit_count < A_HOSTCAN_TX_CAPACITY) &&
        (F_CANUSER_RESULT_OK == F_CanUser_Encode(&g_reply, &p_context->transmit[index])))
    {
        p_context->transmit_count++;
    }
}

/*
 * 说明：查表读取一个参数，始终先验证数据有效性
 * 输入：p_context 上下文，address 参数地址，now_ms 当前时间，p_value 输出32位值
 * 输出：A_HostCan_Code 读取结果
 */
static A_HostCan_Code A_HostCan_ReadParameter(A_HostCan_Context *p_context, uint16_t address,
                                             uint32_t now_ms, uint32_t *p_value)
{
    A_HostCan_Channel g_channel = {0}; // 单路一致快照
    A_HostCan_System g_system = {0};   // 整机一致快照
    uint32_t index = (uint32_t) address & 0x0FU; // 每组中的通道索引
    uint32_t group = (uint32_t) address & 0xFFF0U; // 参数分组
    uint32_t required = 0U; // 本参数所需有效标志
    uint32_t value = 0U;    // 读取位模式
    uint32_t channel = 0U;  // 在线掩码循环索引
    taskENTER_CRITICAL();
    g_system = p_context->system;
    if (index < A_HOSTCAN_CHANNEL_COUNT) { g_channel = p_context->channels[index]; }
    taskEXIT_CRITICAL();

    if ((address >= 0x0100U) && (address <= 0x010EU))
    {
        switch (address)
        {
            case 0x0100U: value = g_system.state; break;
            case 0x0101U: value = g_system.faults; break;
            case 0x0102U:
                taskENTER_CRITICAL();
                for (channel = 0U; channel < A_HOSTCAN_CHANNEL_COUNT; channel++)
                {
                    if (0U != p_context->channels[channel].online) { value |= 1UL << channel; }
                }
                taskEXIT_CRITICAL();
                break;
            case 0x0103U: value = g_system.link; break;
            case 0x0104U:
            case 0x0105U:
                if (0U == g_system.valves_valid) { return A_HOSTCAN_CODE_NOT_READY; }
                value = (0x0104U == address) ? g_system.valve_outputs : g_system.valve_state;
                break;
            case 0x0106U: value = 1U; break; // 本项目参数表版本1
            case 0x0108U: value = A_HOSTCAN_VERSION_MAJOR; break;
            case 0x0109U: value = A_HOSTCAN_VERSION_MINOR; break;
            case 0x010AU: value = A_HOSTCAN_VERSION_PATCH; break;
            case 0x010BU: value = A_HOSTCAN_VERSION_DATE; break;
            case 0x010CU: value = p_context->last_error_address; break;
            case 0x010DU: value = p_context->last_error_code; break;
            case 0x010EU: value = p_context->last_error_function; break;
            default: return A_HOSTCAN_CODE_ADDRESS;
        }
    }
    else if ((address >= 0x0300U) && (address <= 0x0309U))
    {
        if (0U == g_system.valves_valid) { return A_HOSTCAN_CODE_NOT_READY; }
        value = (0x0309U == address) ? g_system.valve_target :
                ((g_system.valve_target >> (address - 0x0300U)) & 1U);
    }
    else
    {
        if (index >= A_HOSTCAN_CHANNEL_COUNT) { return A_HOSTCAN_CODE_ADDRESS; }
        switch (group)
        {
            case 0x0000U:
                required = A_HOSTCAN_VALID_ACTUAL;
                value = F_CanUser_FloatToBits(g_channel.actual_flow);
                if (0U == g_channel.online) { return A_HOSTCAN_CODE_OFFLINE; }
                if ((now_ms - g_channel.sampled_ms) > A_HOSTCAN_DATA_MAX_AGE_MS)
                { return A_HOSTCAN_CODE_STALE; }
                break;
            case 0x0010U:
                required = A_HOSTCAN_VALID_CONFIRMED;
                if (0U == g_channel.online) { return A_HOSTCAN_CODE_OFFLINE; }
                value = F_CanUser_FloatToBits(g_channel.confirmed_flow);
                break;
            case 0x0020U:
                required = A_HOSTCAN_VALID_SCALE;
                value = F_CanUser_FloatToBits(g_channel.full_scale);
                break;
            case 0x0200U:
                required = A_HOSTCAN_VALID_TARGET;
                value = F_CanUser_FloatToBits(g_channel.target_flow);
                break;
            case 0x0110U: value = g_channel.valid_flags; break;
            case 0x0120U:
                required = A_HOSTCAN_VALID_ALARM;
                if (0U == g_channel.online) { return A_HOSTCAN_CODE_OFFLINE; }
                value = g_channel.alarm;
                break;
            case 0x0130U: required = A_HOSTCAN_VALID_UNIT; value = g_channel.unit; break;
            case 0x0140U: required = A_HOSTCAN_VALID_DECIMAL; value = g_channel.decimal_places; break;
            case 0x0150U:
                value = (0U != (g_channel.valid_flags & A_HOSTCAN_VALID_ACTUAL)) ?
                        (now_ms - g_channel.sampled_ms) : UINT32_MAX;
                break;
            case 0x0160U: value = g_channel.initialize_state; break;
            case 0x0170U: required = A_HOSTCAN_VALID_SOURCE; value = g_channel.flow_source; break;
            case 0x0180U:
                required = A_HOSTCAN_VALID_VALVE;
                if (0U == g_channel.online) { return A_HOSTCAN_CODE_OFFLINE; }
                value = g_channel.internal_valve;
                break;
            case 0x0190U: required = A_HOSTCAN_VALID_ADDRESS; value = g_channel.device_address; break;
            default: return A_HOSTCAN_CODE_ADDRESS;
        }
        if ((g_channel.valid_flags & required) != required) { return A_HOSTCAN_CODE_NOT_READY; }
    }
    *p_value = value;
    return A_HOSTCAN_CODE_OK;
}

/*
 * 说明：生成受控写请求，不直接操作SCI或GPIO
 * 输入：p_context 上下文，p_request CAN请求，now_ms 当前时间
 * 输出：A_HostCan_Code 接受结果；OK仅代表已等待执行，不立即回复成功
 */
static A_HostCan_Code A_HostCan_StartWrite(A_HostCan_Context *p_context,
                                          const F_CanUser_Message *p_request, uint32_t now_ms)
{
    A_HostCan_Command g_command = {0}; // 待提交业务命令
    A_HostCan_Channel g_channel = {0}; // 目标通道快照
    A_HostCan_Code result = A_HOSTCAN_CODE_OK; // 参数校验结果
    uint32_t required = A_HOSTCAN_VALID_SCALE | A_HOSTCAN_VALID_UNIT |
                        A_HOSTCAN_VALID_DECIMAL | A_HOSTCAN_VALID_SOURCE; // 换算及控制需要的参数
    float flow = 0.0F; // CAN输入流量工程值
    uint32_t bit = 0U; // 单阀对应位
    if (1U != p_request->count) { return A_HOSTCAN_CODE_VALUE; }
    if (0U == A_HostCan_AddressDefined(p_request->address)) { return A_HOSTCAN_CODE_ADDRESS; }
    if (p_request->address < 0x0200U) { return A_HOSTCAN_CODE_READ_ONLY; }
    g_command.started_ms = now_ms;
    g_command.value = p_request->value;
    taskENTER_CRITICAL();
    if ((p_request->address >= 0x0200U) && (p_request->address <= 0x0205U))
    {
        g_command.operation = A_HOSTCAN_COMMAND_SET_FLOW;
        g_command.index = p_request->address - 0x0200U;
        g_channel = p_context->channels[g_command.index];
        flow = F_CanUser_BitsToFloat(p_request->value);
        if (!isfinite(flow)) { result = A_HOSTCAN_CODE_VALUE; }
        else if (flow < 0.0F) { result = A_HOSTCAN_CODE_RANGE; }
        else if (0U == (p_context->executors & A_HOSTCAN_EXECUTOR_MFC)) { result = A_HOSTCAN_CODE_NOT_READY; }
        else if (0U == g_channel.online) { result = A_HOSTCAN_CODE_OFFLINE; }
        else if (((g_channel.valid_flags & required) != required) ||
                 (2U != g_channel.initialize_state) || (0U != g_channel.flow_source))
        { result = A_HOSTCAN_CODE_NOT_READY; }
        else if (flow > g_channel.full_scale) { result = A_HOSTCAN_CODE_RANGE; }
    }
    else if ((p_request->address >= 0x0300U) && (p_request->address <= 0x0309U))
    {
        g_command.operation = (0x0309U == p_request->address) ?
                              A_HOSTCAN_COMMAND_SET_VALVES : A_HOSTCAN_COMMAND_SET_VALVE;
        g_command.index = p_request->address - 0x0300U;
        g_command.valve_target = p_context->system.valve_target;
        if (A_HOSTCAN_COMMAND_SET_VALVES == g_command.operation)
        {
            g_command.valve_target = p_request->value;
            if (0U == A_HostCan_ValvesLegal(g_command.valve_target)) { result = A_HOSTCAN_CODE_INTERLOCK; }
        }
        else if (p_request->value > 1U) { result = A_HOSTCAN_CODE_VALUE; }
        else
        {
            bit = 1UL << g_command.index;
            if ((6U == g_command.index) || (8U == g_command.index))
            {
                g_command.valve_target &= ~A_HOSTCAN_VALVE_GROUP79;
                if (0U != p_request->value)
                {
                    g_command.valve_target &= ~A_HOSTCAN_VALVE8;
                    g_command.valve_target |= A_HOSTCAN_VALVE_GROUP79;
                }
            }
            else
            {
                g_command.valve_target &= ~bit;
                if (0U != p_request->value)
                {
                    if (7U == g_command.index) { g_command.valve_target &= ~A_HOSTCAN_VALVE_GROUP79; }
                    g_command.valve_target |= bit;
                }
            }
        }
        if ((A_HOSTCAN_CODE_OK == result) &&
            ((0U == (p_context->executors & A_HOSTCAN_EXECUTOR_VALVE)) ||
             (0U == p_context->system.valves_valid)))
        { result = A_HOSTCAN_CODE_NOT_READY; }
    }
    else { result = A_HOSTCAN_CODE_ADDRESS; }
    if ((A_HOSTCAN_CODE_OK == result) && (0U != p_context->command_state)) { result = A_HOSTCAN_CODE_BUSY; }
    if (A_HOSTCAN_CODE_OK == result)
    {
        p_context->next_sequence++;
        if (0U == p_context->next_sequence) { p_context->next_sequence++; }
        g_command.sequence = p_context->next_sequence;
        p_context->command = g_command;
        p_context->requester = *p_request;
        p_context->command_state = 1U;
    }
    taskEXIT_CRITICAL();
    return result;
}

/*
 * 说明：处理一条定向读写请求，未使用的广播和周期功能不产生动作
 * 输入：p_context 上下文，p_request 解码请求，now_ms 当前时间
 * 输出：无
 */
static void A_HostCan_HandleRequest(A_HostCan_Context *p_context, const F_CanUser_Message *p_request,
                                  uint32_t now_ms)
{
    uint32_t values[A_HOSTCAN_MAX_READ_COUNT] = {0}; // 一次读请求的参数快照
    uint32_t index = 0U; // 参数索引
    uint16_t address = p_request->address; // 当前参数地址
    A_HostCan_Code code = A_HOSTCAN_CODE_OK; // 操作结果
    if ((Type_FLOW != p_request->target_type) || (p_context->self_address != p_request->target_address) ||
        (Type_Header != p_request->source_type) || (127U == p_request->source_address) ||
        ((F_CANUSER_READ != p_request->function) && (F_CANUSER_WRITE != p_request->function)))
    {
        p_context->ignored_frames++;
        return;
    }
    if (F_CANUSER_WRITE == p_request->function)
    {
        code = A_HostCan_StartWrite(p_context, p_request, now_ms);//生成受控请求
        if (A_HOSTCAN_CODE_OK != code)
        {
            A_HostCan_RecordError(p_context, p_request, address, (uint32_t) code);
            A_HostCan_QueueReply(p_context, p_request, F_CANUSER_WRITE_RETURN, address, (uint32_t) code);
        }
        return;
    }
    if ((0U == p_request->count) || (p_request->count > A_HOSTCAN_MAX_READ_COUNT) ||
        ((uint32_t) address + p_request->count > 0x0400U))
    { code = A_HOSTCAN_CODE_VALUE; }
    else
    {
        for (index = 0U; index < p_request->count; index++)
        {
            address = (uint16_t) ((uint32_t) p_request->address + index);
            code = A_HostCan_ReadParameter(p_context, address, now_ms, &values[index]);
            if (A_HOSTCAN_CODE_OK != code) { break; }
        }
    }
    if (A_HOSTCAN_CODE_OK != code)
    {
        A_HostCan_RecordError(p_context, p_request, address, (uint32_t) code);
        return; // 保持原0x07仅返回参数；无统一读错误帧，错误由0x010C～0x010E查询
    }
    for (index = 0U; index < p_request->count; index++)
    {
        A_HostCan_QueueReply(p_context, p_request, F_CANUSER_READ_RETURN,
                            (uint16_t) ((uint32_t) p_request->address + index), values[index]);
    }
}

/*
 * 说明：有界推进CAN发送，发送超时清理旧回复并执行退避恢复
 * 输入：p_context 上下文，now_ms 当前时间
 * 输出：无
 */
static void A_HostCan_ServiceTransmit(A_HostCan_Context *p_context, uint32_t now_ms)
{
    F_HostCan_Result state = F_HostCan_GetTransmitState(&p_context->transport); // 硬件状态
    if ((F_HOSTCAN_ERROR == state) || ((F_HOSTCAN_BUSY == state) && (0U != p_context->transmit_active) &&
        ((now_ms - p_context->transmit_started_ms) >= A_HOSTCAN_TX_TIMEOUT_MS)))
    {
        if (0U == p_context->recovering)
        {
            p_context->recovering = 1U;
            p_context->failed_transmissions++;
            p_context->transmit_count = 0U;
            p_context->transmit_active = 0U;
            p_context->recovery_ms = now_ms - A_HOSTCAN_TX_TIMEOUT_MS;
            taskENTER_CRITICAL();
            p_context->command_state = 0U; // 旧主机请求不在恢复后重新执行
            taskEXIT_CRITICAL();
        }
    }
    if (0U != p_context->recovering)
    {
        if ((now_ms - p_context->recovery_ms) >= A_HOSTCAN_TX_TIMEOUT_MS)
        {
            p_context->recovery_ms = now_ms;
            if (F_HOSTCAN_OK == F_HostCan_Recover(&p_context->transport)) { p_context->recovering = 0U; }
        }
        return;
    }
    if (0U != p_context->transmit_active)
    {
        if (F_HOSTCAN_OK == state)
        {
            p_context->transmit_read = (p_context->transmit_read + 1U) % A_HOSTCAN_TX_CAPACITY;
            p_context->transmit_count--;
            p_context->transmit_active = 0U;
        }
        else { return; }
    }
    if (0U != p_context->transmit_count)
    {
        state = F_HostCan_Send(&p_context->transport, &p_context->transmit[p_context->transmit_read]);
        if (F_HOSTCAN_OK == state)
        {
            p_context->transmit_active = 1U;
            p_context->transmit_started_ms = now_ms;
        }
    }
}

/*
 * 说明：提交执行完成或超时的写回复，不把领取请求当成成功
 * 输入：p_context 上下文，now_ms 当前时间
 * 输出：无
 */
static void A_HostCan_ServiceCommand(A_HostCan_Context *p_context, uint32_t now_ms)
{
    F_CanUser_Message g_request = {0}; // 保存原请求来源
    uint32_t completed = 0U; // 本轮是否生成回复
    uint32_t code = 0U;      // 业务结果
    taskENTER_CRITICAL();
    if (((1U == p_context->command_state) || (2U == p_context->command_state)) &&
        ((now_ms - p_context->command.started_ms) >= A_HOSTCAN_COMMAND_TIMEOUT_MS))
    {
        p_context->command_result = A_HOSTCAN_CODE_EXECUTION_TIMEOUT;
        p_context->command_state = 3U;
    }
    if ((3U == p_context->command_state) && (p_context->transmit_count < A_HOSTCAN_TX_CAPACITY))
    {
        g_request = p_context->requester;
        code = p_context->command_result;
        p_context->command_state = 0U;
        completed = 1U;
    }
    taskEXIT_CRITICAL();
    if (0U != completed)
    {
        if (0U != code) { A_HostCan_RecordError(p_context, &g_request, g_request.address, code); }
        A_HostCan_QueueReply(p_context, &g_request, F_CANUSER_WRITE_RETURN, g_request.address, code);
    }
}

/*
 * 说明：初始化CAN业务，仅由HostCanStack首次调用
 * 输入：p_context 零初始化上下文，self_address 主板地址
 * 输出：uint32_t 非0成功
 */
uint32_t A_HostCan_Initialize(A_HostCan_Context *p_context, uint8_t self_address)
{
    if ((NULL == p_context) || (self_address >= 127U)) { return 0U; }
    if (0U != p_context->initialized) { return 1U; }
    if (F_HOSTCAN_OK != F_HostCan_Initialize(&p_context->transport)) { return 0U; }
    p_context->self_address = self_address;
    p_context->initialized = 1U;
    return 1U;
}

/*
 * 说明：每次最多处理4个请求，发送拥塞时停止取新请求
 * 输入：p_context 上下文，now_ms 当前时间
 * 输出：无
 */
void A_HostCan_Process(A_HostCan_Context *p_context, uint32_t now_ms)
{
    F_CanUser_Frame g_frame = {0};     // 接收帧
    F_CanUser_Message g_request = {0}; // 解码请求
    uint32_t budget = 4U;             // 每次处理预算
    if ((NULL == p_context) || (0U == p_context->initialized)) { return; }
    A_HostCan_ServiceTransmit(p_context, now_ms);
    if (0U != p_context->recovering) { return; }
    A_HostCan_ServiceCommand(p_context, now_ms);
    while ((budget > 0U) &&
           ((A_HOSTCAN_TX_CAPACITY - p_context->transmit_count) >= A_HOSTCAN_MAX_READ_COUNT))
    {
        if (F_HOSTCAN_OK != F_HostCan_Receive(&p_context->transport, &g_frame)) { break; }
        budget--;
        if (F_CANUSER_RESULT_OK != F_CanUser_Decode(&g_frame, &g_request))
        {
            p_context->invalid_frames++;
            continue;
        }
        A_HostCan_HandleRequest(p_context, &g_request, now_ms);
    }
}

/*
 * 说明：发布经过设备协议校验的单通道状态
 * 输入：p_context 上下文，index 通道，p_channel 完整快照
 * 输出：uint32_t 非0成功
 */
uint32_t A_HostCan_PublishChannel(A_HostCan_Context *p_context, uint32_t index, const A_HostCan_Channel *p_channel)
{
    if ((NULL == p_context) || (NULL == p_channel) || (index >= A_HOSTCAN_CHANNEL_COUNT) ||
        (0U == p_context->initialized)) { return 0U; }
    if (((0U != (p_channel->valid_flags & A_HOSTCAN_VALID_ACTUAL)) && !isfinite(p_channel->actual_flow)) ||
        ((0U != (p_channel->valid_flags & A_HOSTCAN_VALID_CONFIRMED)) && !isfinite(p_channel->confirmed_flow)) ||
        ((0U != (p_channel->valid_flags & A_HOSTCAN_VALID_TARGET)) && !isfinite(p_channel->target_flow)) ||
        ((0U != (p_channel->valid_flags & A_HOSTCAN_VALID_SCALE)) &&
         (!isfinite(p_channel->full_scale) || (p_channel->full_scale <= 0.0F))) ||
        ((0U != (p_channel->valid_flags & A_HOSTCAN_VALID_DECIMAL)) && (p_channel->decimal_places > 3U)) ||
        ((0U != (p_channel->valid_flags & A_HOSTCAN_VALID_UNIT)) && (p_channel->unit > 1U)))
    { return 0U; }
    taskENTER_CRITICAL();
    p_context->channels[index] = *p_channel;
    taskEXIT_CRITICAL();
    return 1U;
}

/*
 * 说明：发布ControlTask输出快照，目标和输出都必须满足联锁
 * 输入：p_context 上下文，p_system 系统快照
 * 输出：uint32_t 非0成功
 */
uint32_t A_HostCan_PublishSystem(A_HostCan_Context *p_context, const A_HostCan_System *p_system)
{
    uint32_t link = 0U; // 链路由MfcTask单独发布，保留当前值
    if ((NULL == p_context) || (NULL == p_system) || (0U == p_context->initialized) ||
        (0U == A_HostCan_ValvesLegal(p_system->valve_outputs)) ||
        (0U == A_HostCan_ValvesLegal(p_system->valve_target))) { return 0U; }
    taskENTER_CRITICAL();
    link = p_context->system.link;
    p_context->system = *p_system;
    p_context->system.link = link;
    taskEXIT_CRITICAL();
    return 1U;
}

/*
 * 说明：仅更新链路字段，MfcTask不得覆盖九阀及整机状态
 * 输入：p_context 上下文，link 下行链路状态
 * 输出：uint32_t 非0成功
 */
uint32_t A_HostCan_PublishMfcLink(A_HostCan_Context *p_context, uint32_t link)
{
    if ((NULL == p_context) || (0U == p_context->initialized) || (link > 3U)) { return 0U; }
    taskENTER_CRITICAL();
    p_context->system.link = link;
    taskEXIT_CRITICAL();
    return 1U;
}

/*
 * 说明：设置业务执行入口就绪掩码，不设置即明确拒绝写操作
 * 输入：p_context 上下文，executors 可执行类型
 * 输出：无
 */
void A_HostCan_SetExecutors(A_HostCan_Context *p_context, uint32_t executors)
{
    if ((NULL == p_context) || (0U == p_context->initialized)) { return; }
    taskENTER_CRITICAL();
    p_context->executors = executors & (A_HOSTCAN_EXECUTOR_MFC | A_HOSTCAN_EXECUTOR_VALVE);
    if (((1U == p_context->command_state) || (2U == p_context->command_state)) &&
        (((A_HOSTCAN_COMMAND_SET_FLOW == p_context->command.operation) &&
          (0U == (p_context->executors & A_HOSTCAN_EXECUTOR_MFC))) ||
         ((A_HOSTCAN_COMMAND_SET_FLOW != p_context->command.operation) &&
          (0U == (p_context->executors & A_HOSTCAN_EXECUTOR_VALVE)))))
    {
        p_context->command_result = A_HOSTCAN_CODE_NOT_READY;
        p_context->command_state = 3U;
    }
    taskEXIT_CRITICAL();
}

/*
 * 说明：CAN任务提取本地待发送请求，只允许入队一次
 * 输入：p_context 上下文，p_command 输出命令
 * 输出：uint32_t 非0取得请求
 */
uint32_t A_HostCan_TakeCommand(A_HostCan_Context *p_context, A_HostCan_Command *p_command)
{
    uint32_t taken = 0U; // 本次是否取得命令
    if ((NULL == p_context) || (NULL == p_command) || (0U == p_context->initialized)) { return 0U; }
    taskENTER_CRITICAL();
    if (1U == p_context->command_state)
    {
        *p_command = p_context->command;
        p_context->command_state = 2U;
        taken = 1U;
    }
    taskEXIT_CRITICAL();
    return taken;
}

/*
 * 说明：CAN任务核对本地请求号、硬件状态和期限，结果通过队列发布
 * 输入：p_context 上下文，sequence 请求号，now_ms 当前时间
 * 输出：uint32_t 非0仍有效
 */
uint32_t A_HostCan_CommandActive(A_HostCan_Context *p_context, uint32_t sequence, uint32_t now_ms)
{
    uint32_t active = 0U; // 有效标志
    if ((NULL == p_context) || (0U == p_context->initialized)) { return 0U; }
    taskENTER_CRITICAL();
    active = ((2U == p_context->command_state) && (sequence == p_context->command.sequence) &&
              (0U == p_context->recovering) &&
              (F_HOSTCAN_ERROR != F_HostCan_GetTransmitState(&p_context->transport)) &&
              ((now_ms - p_context->command.started_ms) < A_HOSTCAN_COMMAND_TIMEOUT_MS)) ? 1U : 0U;
    taskEXIT_CRITICAL();
    return active;
}

/*
 * 说明：保存实际执行结果，旧请求和迟到结果不能完成新请求
 * 输入：p_context 上下文，sequence 请求号，code 业务码，now_ms 当前时间
 * 输出：uint32_t 非0接收
 */
uint32_t A_HostCan_CompleteCommand(A_HostCan_Context *p_context, uint32_t sequence, uint8_t code, uint32_t now_ms)
{
    uint32_t accepted = 0U; // 是否接受结果
    if ((NULL == p_context) || (0U == p_context->initialized)) { return 0U; }
    taskENTER_CRITICAL();
    if ((2U == p_context->command_state) && (sequence == p_context->command.sequence) &&
        ((now_ms - p_context->command.started_ms) < A_HOSTCAN_COMMAND_TIMEOUT_MS))
    {
        p_context->command_result = code;
        p_context->command_state = 3U;
        accepted = 1U;
    }
    taskEXIT_CRITICAL();
    return accepted;
}
