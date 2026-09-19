/*
 * Created on: 2026年9月15日
 * Author: CI
 */
#include "A_Control.h"

/*
 * 说明：绑定六个交接队列并初始化九阀，禁止重置运行中的控制上下文
 * 输入：p_context 上下文，p_queues 队列句柄集合
 * 输出：uint32_t 非0成功
 */
uint32_t A_Control_Initialize(A_Control_Context *p_context, const A_Control_Queue_Set *p_queues)
{
    if (NULL == p_context || NULL == p_queues || NULL == p_context->p_valve || NULL == p_queues->host_command ||
        NULL == p_queues->host_result || NULL == p_queues->command ||
        NULL == p_queues->result || NULL == p_queues->host_state || NULL == p_queues->valve_state)
    {
        return 0U;
    }
    if (p_context->initialized)
    {
        return 1U;
    }
    p_context->queues = *p_queues;
    (void) A_Valve_Initialize(p_context->p_valve); // 阀故障通过状态发布，不能阻止MFC任务继续采集
    p_context->initialized = 1U;
    return 1U;
}

/*
 * 说明：校验队列传来的请求状态；CAN任务停止更新后不得持续使用旧授权
 * 输入：p_state 本地副本，sequence 请求号，started_tick 起点，now 当前节拍
 * 输出：uint32_t 非0有效
 */
uint32_t A_Control_RequestValid(const A_Control_Host_State *p_state, uint32_t sequence,
                               TickType_t started_tick, TickType_t now)
{
    // 第1步：必须已经收到CAN任务给出的有效状态。
    if (p_state == NULL || p_state->valid == 0U)
    {
        return 0U;
    }

    // 第2步：请求号和起点都要相同，避免把新请求的状态用于旧命令。
    if (sequence == 0U || p_state->sequence != sequence || p_state->started_tick != started_tick)
    {
        return 0U;
    }

    // 第3步：CAN任务超过20ms没有更新状态，不能继续使用旧授权。
    if ((TickType_t) (now - p_state->updated_tick) >= pdMS_TO_TICKS(A_CONTROL_HOST_STATE_MAX_AGE_MS))
    {
        return 0U;
    }

    // 第4步：排队和执行合计不能超过3000ms，转发队列不能重新计时。
    if ((TickType_t) (now - started_tick) >= pdMS_TO_TICKS(A_HOSTCAN_COMMAND_TIMEOUT_MS))
    {
        return 0U;
    }

    return 1U;
}

/*
 * 说明：把模块内部结果映射为原CAN_USER的uint8业务结果码
 * 输入：code MFC执行结果
 * 输出：uint8_t CAN_USER业务码
 */
static uint8_t A_Control_MapResult(A_MFC_CommandCode code)
{
    switch (code)
    {
        case A_MFC_COMMAND_OK:
            return A_HOSTCAN_CODE_OK;
        case A_MFC_COMMAND_OFFLINE:
            return A_HOSTCAN_CODE_OFFLINE;
        case A_MFC_COMMAND_NOT_READY:
            return A_HOSTCAN_CODE_NOT_READY;
        case A_MFC_COMMAND_VALUE:
            return A_HOSTCAN_CODE_VALUE;
        case A_MFC_COMMAND_RANGE:
            return A_HOSTCAN_CODE_RANGE;
        case A_MFC_COMMAND_DEVICE_NG:
            return A_HOSTCAN_CODE_DEVICE_NG;
        case A_MFC_COMMAND_TIMEOUT:
            return A_HOSTCAN_CODE_DOWNSTREAM_TIMEOUT;
        case A_MFC_COMMAND_EXPIRED:
            return A_HOSTCAN_CODE_EXECUTION_TIMEOUT;
        case A_MFC_COMMAND_PROTOCOL:
            return A_HOSTCAN_CODE_DOWNSTREAM_PROTOCOL;
        case A_MFC_COMMAND_VERIFY:
            return A_HOSTCAN_CODE_VERIFY;
        default:
            return A_HOSTCAN_CODE_DOWNSTREAM_DRIVER;
    }
}

/*
 * 说明：映射阀门执行结果，不把GPIO成功解释为机械阀位确认
 * 输入：result 阀门模块结果
 * 输出：uint32_t CAN_USER业务码
 */
static uint32_t A_Control_MapValveResult(F_Valve_Result result)
{
    switch (result)
    {
        case F_VALVE_OK:
            return A_HOSTCAN_CODE_OK;
        case F_VALVE_INVALID_TARGET:
            return A_HOSTCAN_CODE_INTERLOCK;
        case F_VALVE_DRIVER_ERROR:
            return A_HOSTCAN_CODE_VALVE_DRIVER;
        default:
            return A_HOSTCAN_CODE_NOT_READY;
    }
}

/*
 * 说明：先发布本任务输出快照，再发送结果，CAN查询不访问Control私有状态
 * 输入：p_context 本任务上下文
 * 输出：无，队列长度1，始终覆盖为最新完整状态
 */
static void A_Control_PublishValveState(A_Control_Context *p_context)
{
    A_Valve_Context *p_valve = p_context->p_valve; // 本任务独占状态
    A_HostCan_System g_snapshot = {0}; // 队列复制内容，不传上下文指针
    g_snapshot.valve_outputs = p_valve->outputs;
    g_snapshot.valve_target = p_valve->target;
    g_snapshot.valve_state = p_valve->fault ? 2U : (p_valve->pull_in_mask ? 1U : 0U);
    g_snapshot.valves_valid = (p_valve->initialized && !p_valve->fault) ? 1U : 0U;
    g_snapshot.faults = p_valve->fault ? A_HOSTCAN_FAULT_VALVE_DRIVER : 0U;
    (void) xQueueOverwrite(p_context->queues.valve_state, &g_snapshot);
}

/*
 * 说明：只通过队列收发；结果未入队时保留，不覆盖且不领取下一笔命令
 * 输入：p_context 本任务独占上下文，now 当前节拍
 * 输出：无
 */
void A_Control_Process(A_Control_Context *p_context, TickType_t now)
{
    A_HostCan_Command g_host_command = {0}; // CAN已解析请求
    A_MFC_Command g_command = {0}; // 发给MfcTask的命令副本
    A_MFC_CommandResult g_result = {0}; // 收到的执行结果
    F_Valve_Result valve_result = F_VALVE_OK; // 本轮阀门执行状态
    uint32_t valve_cancelled = 0U; // 未完成阀请求是否失效
    if (NULL == p_context || !p_context->initialized)
    {
        return;
    }
    // 第1步：从CAN状态队列取最新消息。队列为空时保留本地副本，随后检查其年龄。
    (void) xQueueReceive(p_context->queues.host_state, &p_context->host_state, 0U);

    // 阀门计时必须先运行；CAN队列拥塞或MFC写入等待都不能阻止200ms后撤销12V。
    if (p_context->active_sequence != 0U && p_context->active_operation != A_HOSTCAN_COMMAND_SET_FLOW &&
        !A_Control_RequestValid(&p_context->host_state, p_context->active_sequence,
            p_context->active_started, now))
    {
        (void) A_Valve_Cancel(p_context->p_valve, now);
        valve_cancelled = 1U;
    }
    valve_result = A_Valve_Process(p_context->p_valve, now);
    if (p_context->active_sequence != 0U && p_context->active_operation != A_HOSTCAN_COMMAND_SET_FLOW &&
        (valve_cancelled || valve_result != F_VALVE_OK || p_context->p_valve->pull_in_mask == 0U))
    {
        p_context->pending_result.sequence = p_context->active_sequence;
        p_context->pending_result.code = A_Control_MapValveResult(valve_result);
        if (valve_cancelled && valve_result == F_VALVE_OK)
        {
            p_context->pending_result.code = A_HOSTCAN_CODE_EXECUTION_TIMEOUT;
        }
        p_context->result_pending = 1U;
        p_context->active_sequence = 0U;
    }

    // 第2步：先收上一笔MFC执行结果；已有结果尚未送给CAN时，不能覆盖它。
    if (!p_context->result_pending && pdPASS == xQueueReceive(p_context->queues.result, &g_result, 0U))
    {
        if (g_result.sequence == p_context->active_sequence && p_context->active_operation == A_HOSTCAN_COMMAND_SET_FLOW)
        {
            p_context->pending_result.sequence = g_result.sequence;
            p_context->pending_result.code = A_Control_MapResult(g_result.code);
            p_context->result_pending = 1U;
            p_context->active_sequence = 0U;
        }
    }
    A_Control_PublishValveState(p_context);
    // 第3步：把执行结果送给CAN任务。队列满就退出本轮，下次继续尝试。
    if (p_context->result_pending)
    {
        if (pdPASS != xQueueSendToBack(p_context->queues.host_result, &p_context->pending_result, 0U))
        {
            return;
        }
        p_context->result_pending = 0U;
    }
    // 第4步：上一笔MFC写入或阀门吸合尚未完成时，暂不领取新的CAN命令。
    if (p_context->active_sequence != 0U)
    {
        return;
    }
    if (pdPASS != xQueueReceive(p_context->queues.host_command, &g_host_command, 0U))
    {
        return; // 没有新命令，本轮结束；等待时间为0，不阻塞任务。
    }

    // 第5步：检查请求是否仍有效。出错时保存结果，由下一轮送入CAN结果队列。
    p_context->pending_result.sequence = g_host_command.sequence;
    if (!A_Control_RequestValid(&p_context->host_state, g_host_command.sequence, g_host_command.started_ms, now))
    {
        p_context->pending_result.code = A_HOSTCAN_CODE_EXECUTION_TIMEOUT;
        p_context->result_pending = 1U;
        return;
    }
    if (g_host_command.operation == A_HOSTCAN_COMMAND_SET_VALVE ||
        g_host_command.operation == A_HOSTCAN_COMMAND_SET_VALVES)
    {
        // 此处再检查联锁；CAN仅形成目标，只有ControlTask可以驱动阀门。
        valve_result = A_Valve_SetTarget(p_context->p_valve, g_host_command.valve_target, now);
        A_Control_PublishValveState(p_context);
        if (valve_result == F_VALVE_OK && p_context->p_valve->pull_in_mask != 0U)
        {
            p_context->active_sequence = g_host_command.sequence;
            p_context->active_operation = g_host_command.operation;
            p_context->active_started = g_host_command.started_ms;
        }
        else
        {
            p_context->pending_result.code = A_Control_MapValveResult(valve_result);
            p_context->result_pending = 1U;
        }
        return;
    }
    if (g_host_command.operation != A_HOSTCAN_COMMAND_SET_FLOW)
    {
        p_context->pending_result.code = A_HOSTCAN_CODE_NOT_READY;
        p_context->result_pending = 1U;
        return;
    }
    // 第6步：把CAN的32位原始数值转成float流量，再组装MFC执行命令。
    g_command.sequence = g_host_command.sequence;
    g_command.index = g_host_command.index;
    g_command.operation = A_MFC_COMMAND_SET_FLOW;
    g_command.target_flow = F_CanUser_BitsToFloat(g_host_command.value);
    g_command.started_tick = g_host_command.started_ms;
    g_command.timeout_ticks = A_HOSTCAN_COMMAND_TIMEOUT_MS;
    // 第7步：只在成功入队后记录执行中的请求号；入队成功不代表仪器已经写成功。
    if (pdPASS == xQueueSendToBack(p_context->queues.command, &g_command, 0U))
    {
        p_context->active_sequence = g_command.sequence;
        p_context->active_operation = A_HOSTCAN_COMMAND_SET_FLOW;
    }
    else
    {
        p_context->pending_result.code = A_HOSTCAN_CODE_BUSY;
        p_context->result_pending = 1U;
    }
}
