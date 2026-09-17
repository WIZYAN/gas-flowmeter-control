/*
 * Created on: 2026年9月15日
 * Author: CI
 */
#include "A_MFC.h"
#include <math.h>
#include <float.h>

#if configTICK_RATE_HZ != 1000
#error "MFC polling currently requires a 1 ms RTOS tick."
#endif

/*
 * 说明：生成全部启用的六路默认地址
 * 输入：p_config 输出配置
 * 输出：无
 */
void A_MFC_DefaultConfig(A_MFC_Config *p_config)
{
    uint32_t index = 0U; // 通道索引
    if (NULL == p_config)
    {
        return;
    }
    for (index = 0U; index < A_MFC_CHANNEL_COUNT; index++)
    {
        p_config->addresses[index] = (uint16_t) (index + 1U);
    }
}

/*
 * 说明：清除离线通道缓存有效位，并从量程读取重新开始
 * 输入：p_channel 通道，now 当前节拍，result 故障原因
 * 输出：无
 */
static void A_MFC_MarkOffline(A_MFC_Channel *p_channel, TickType_t now, A_EX201_Result result)
{
    p_channel->online = 0U;
    p_channel->flow_valid = 0U;
    p_channel->device_info.valid_flags = 0U;
    p_channel->initialize_state = A_MFC_INIT_FAILED;
    p_channel->initialize_step = 0U;
    p_channel->status_step = 0U;
    p_channel->prefer_status = 0U;
    p_channel->retry_tick = now;
    p_channel->last_error = (uint32_t) result;
    p_channel->revision++;
}

/*
 * 说明：校验六个非广播且互不重复的设备地址
 * 输入：p_context 上下文，p_config 六路配置，now 当前节拍
 * 输出：uint32_t 非0成功
 */
uint32_t A_MFC_Initialize(A_MFC_Context *p_context, const A_MFC_Config *p_config, TickType_t now)
{
    uint32_t index = 0U; // 正在检查的通道索引
    uint32_t previous = 0U; // 已检查过的通道，用于查重地址
    if ((NULL == p_context) || (NULL == p_config) || (NULL == p_context->p_channels) ||
        (NULL == p_context->p_transaction) || (0U != p_context->configured))
    {
        return 0U;
    }
    if (p_context->p_transaction->p_function == NULL)
    {
        return 0U; // 事务的组帧状态未绑定，不能进入后续硬件初始化。
    }
    for (index = 0U; index < A_MFC_CHANNEL_COUNT; index++)
    {
        if ((p_config->addresses[index] < EX201_ADDRESS_MIN) ||
            (p_config->addresses[index] > EX201_ADDRESS_MAX))
        {
            return 0U;
        }
        for (previous = 0U; previous < index; previous++)
        {
            if (p_config->addresses[index] == p_config->addresses[previous])
            {
                return 0U;
            }
        }
    }
    for (index = 0U; index < A_MFC_CHANNEL_COUNT; index++)
    {
        p_context->p_channels[index].address = p_config->addresses[index];
        p_context->p_channels[index].initialize_state = A_MFC_INIT_RUNNING;
        p_context->p_channels[index].revision = 1U;
        p_context->p_channels[index].retry_tick = now;
    }
    p_context->configured = 1U;
    return 1U;
}

/*
 * 说明：以枚举选择已存在的EX201只读业务接口
 * 输入：p_context 唯一事务，address 设备地址，operation 只读操作，now 当前节拍
 * 输出：A_EX201_Result 启动结果
 */
static A_EX201_Result A_MFC_StartRead(A_EX201_Context *p_context, uint16_t address,
                                     A_MFC_ReadOperation operation, TickType_t now)
{
    switch (operation)
    {
        case A_MFC_READ_SCALE:
            return A_EX201_ReadFullScale(p_context, address, now);
        case A_MFC_READ_DECIMAL:
            return A_EX201_ReadDecimalPlaces(p_context, address, now);
        case A_MFC_READ_UNIT:
            return A_EX201_ReadFlowUnit(p_context, address, now);
        case A_MFC_READ_ACTUAL:
            return A_EX201_ReadActualFlow(p_context, address, now);
        case A_MFC_READ_CONFIRMED:
            return A_EX201_ReadSetFlow(p_context, address, now);
        case A_MFC_READ_SOURCE:
            return A_EX201_ReadFlowSource(p_context, address, now);
        case A_MFC_READ_SETTING:
            return A_EX201_ReadValveSetting(p_context, address, now);
        case A_MFC_READ_VALVE:
            return A_EX201_ReadValveState(p_context, address, now);
        case A_MFC_READ_ALARM:
            return A_EX201_ReadAlarmState(p_context, address, now);
        default:
            return A_EX201_RESULT_INVALID_ARGUMENT;
    }
}

/*
 * 说明：失败时使本次查询字段立即失效，禁止把旧数据当作本次结果
 * 输入：p_channel 通道，operation 失败的操作
 * 输出：无
 */
static void A_MFC_InvalidateField(A_MFC_Channel *p_channel, A_MFC_ReadOperation operation)
{
    static const uint32_t s_info_flags[A_MFC_READ_COUNT] = {
        A_EX201_DEVICE_INFO_FULL_SCALE_VALID, A_EX201_DEVICE_INFO_DECIMAL_VALID,
        A_EX201_DEVICE_INFO_UNIT_VALID, 0U, 0U, A_EX201_DEVICE_INFO_SOURCE_VALID,
        A_EX201_DEVICE_INFO_VALVE_SETTING_VALID, A_EX201_DEVICE_INFO_VALVE_STATE_VALID,
        A_EX201_DEVICE_INFO_ALARM_VALID
    }; // 初始化操作与设备信息有效位的对应关系
    p_channel->device_info.valid_flags &= ~s_info_flags[operation];
    if (A_MFC_READ_ACTUAL == operation)
    {
        p_channel->flow_valid &= ~A_MFC_VALID_ACTUAL;
    }
    if (A_MFC_READ_CONFIRMED == operation)
    {
        p_channel->flow_valid &= ~A_MFC_VALID_CONFIRMED;
    }
}

/*
 * 说明：硬件故障时使六路离线，后续按退避间隔恢复共享传输
 * 输入：p_context 上下文，now 当前节拍，result 原因
 * 输出：无
 */
static void A_MFC_TransportFailed(A_MFC_Context *p_context, TickType_t now, A_EX201_Result result)
{
    uint32_t index = 0U; // 通道索引
    p_context->transport_ready = 0U;
    p_context->transport_attempted = 1U;
    p_context->transport_retry_tick = now;
    p_context->valid_responses = 0U;
    for (index = 0U; index < A_MFC_CHANNEL_COUNT; index++)
    {
        A_MFC_MarkOffline(&p_context->p_channels[index], now, result);
    }
}

/*
 * 说明：消费事务结果、更新本通道状态，并在失败后让出总线
 * 输入：p_context 上下文，result 当前结果，now 当前节拍
 * 输出：无
 */
static void A_MFC_Finish(A_MFC_Context *p_context, A_EX201_Result result, TickType_t now)
{
    A_MFC_Channel *p_channel = &p_context->p_channels[p_context->active_channel]; // 当前通道
    p_context->active = 0U;
    p_channel->last_error = (uint32_t) result;
    p_channel->revision++;
    if (A_EX201_RESULT_OK == result)
    {
        p_channel->online = 1U;
        p_channel->consecutive_failures = 0U;
        if (p_context->valid_responses < 2U)
        {
            p_context->valid_responses++;
        }
        if (A_MFC_INIT_RUNNING == p_channel->initialize_state)
        {
            p_channel->initialize_step++;
            if (p_channel->initialize_step == (uint32_t) A_MFC_READ_COUNT)
            {
                p_channel->initialize_state = A_MFC_INIT_READY;
                p_channel->status_attempt_tick = now;
            }
        }
        return;
    }
    A_MFC_InvalidateField(p_channel, p_context->active_operation);
    p_channel->consecutive_failures++;
    if ((A_MFC_INIT_READY != p_channel->initialize_state) ||
        (p_channel->consecutive_failures >= A_MFC_FAILURE_LIMIT))
    {
        A_MFC_MarkOffline(p_channel, now, result);
    }
    p_context->guard_active = 1U;
    p_context->guard_tick = now;
    if (A_EX201_RESULT_OK != A_EX201_Recover(p_context->p_transaction))
    {
        A_MFC_TransportFailed(p_context, now, A_EX201_RESULT_RECOVERY_ERROR);
    }
}

/*
 * 说明：选择通道下一项操作；流量与慢速参数同时到期时交替服务
 * 输入：p_channel 通道，now 当前节拍，p_operation 输出操作
 * 输出：uint32_t 非0表示有到期任务
 */
static uint32_t A_MFC_SelectRead(A_MFC_Channel *p_channel, TickType_t now,
                                A_MFC_ReadOperation *p_operation)
{
    uint32_t actual_due = 0U; // 实际流量是否到查询时间
    uint32_t status_due = 0U; // 慢速参数是否到查询时间
    if (A_MFC_INIT_FAILED == p_channel->initialize_state)
    {
        if ((TickType_t) (now - p_channel->retry_tick) < A_MFC_RETRY_MS)
        {
            return 0U;
        }
        p_channel->initialize_state = A_MFC_INIT_RUNNING;
        p_channel->consecutive_failures = 0U;
        p_channel->revision++;
    }
    if (A_MFC_INIT_RUNNING == p_channel->initialize_state)
    {
        *p_operation = (A_MFC_ReadOperation) p_channel->initialize_step;
    }
    else
    {
        actual_due = ((TickType_t) (now - p_channel->actual_attempt_tick) >= A_MFC_ACTUAL_PERIOD_MS);
        status_due = ((TickType_t) (now - p_channel->status_attempt_tick) >= A_MFC_STATUS_SLOT_MS);
        if ((0U == actual_due) && (0U == status_due))
        {
            return 0U;
        }
        if ((0U != status_due) && ((0U == actual_due) || (0U != p_channel->prefer_status)))
        {
            *p_operation = (A_MFC_ReadOperation) ((uint32_t) A_MFC_READ_CONFIRMED + p_channel->status_step);
            p_channel->status_step = (p_channel->status_step + 1U) % 5U;
            p_channel->status_attempt_tick = now;
            p_channel->prefer_status = 0U;
        }
        else
        {
            *p_operation = A_MFC_READ_ACTUAL;
            p_channel->prefer_status = 1U;
        }
    }
    if (A_MFC_READ_ACTUAL == *p_operation)
    {
        p_channel->actual_attempt_tick = now;
    }
    return 1U;
}

/*
 * 说明：推进共享EX201事务，每次最多发起一个请求，没有阻塞等待
 * 输入：p_context 上下文，now 当前节拍
 * 输出：无
 */
void A_MFC_Process(A_MFC_Context *p_context, TickType_t now)
{
    A_EX201_Result result = A_EX201_RESULT_OK; // 事务结果
    A_MFC_Channel *p_channel = NULL; // 当前通道
    uint32_t checked = 0U; // 本轮已经检查的通道数量，最多六个
    uint32_t index = 0U; // 当前检查的通道索引
    int32_t mantissa = 0; // 已解析流量尾数
    if ((NULL == p_context) || (0U == p_context->configured))
    {
        return;
    }
    if ((p_context->write_state >= A_MFC_WRITE_SOURCE) &&
        (p_context->write_state < A_MFC_WRITE_DONE))
    {
        return;
    }
    // 第1步：串口尚未就绪时，按退避时间初始化或恢复共享事务。
    if (0U == p_context->transport_ready)
    {
        if ((0U != p_context->transport_attempted) &&
            ((TickType_t) (now - p_context->transport_retry_tick) < A_MFC_RETRY_MS))
        {
            return;
        }
        if (0U == p_context->p_transaction->initialized)
        {
            result = A_EX201_Initialize(p_context->p_transaction);
        }
        else
        {
            result = A_EX201_Recover(p_context->p_transaction);
        }
        if (A_EX201_RESULT_OK != result)
        {
            A_MFC_TransportFailed(p_context, now, result);
            return;
        }
        p_context->transport_ready = 1U;
    }
    // 第2步：上一笔查询还没结束，先接收和解析它，本轮不能再发送另一笔。
    if (0U != p_context->active)
    {
        A_EX201_Process(p_context->p_transaction, now);//当有事务时，rs485接收，检查
        p_channel = &p_context->p_channels[p_context->active_channel];
        if ((A_MFC_READ_ACTUAL == p_context->active_operation) ||
            (A_MFC_READ_CONFIRMED == p_context->active_operation))
        {
            result = A_EX201_GetFlowResult(p_context->p_transaction, &mantissa);
            if (A_EX201_RESULT_OK == result)
            {
                if (A_MFC_READ_ACTUAL == p_context->active_operation)
                {
                    p_channel->actual_mantissa = mantissa;
                    p_channel->flow_valid |= A_MFC_VALID_ACTUAL;
                    p_channel->sampled_tick = now;
                }
                else
                {
                    p_channel->confirmed_mantissa = mantissa;
                    p_channel->flow_valid |= A_MFC_VALID_CONFIRMED;
                }
            }
        }
        else
        {
            result = A_EX201_GetDeviceInfoResult(p_context->p_transaction, &p_channel->device_info);
        }
        if (A_EX201_RESULT_BUSY == result)
        {
            return;
        }
        A_MFC_Finish(p_context, result, now);
        return;
    }
    // 第3步：通信出错后先等待总线静默，避免立即把迟到响应当作新响应。
    if (0U != p_context->guard_active)
    {
        if ((TickType_t) (now - p_context->guard_tick) < A_MFC_ERROR_GUARD_MS)
        {
            return;
        }
        p_context->guard_active = 0U;
    }
    // 第4步：从next_channel开始轮转，找到到期通道后只启动一笔查询。
    for (checked = 0U; checked < A_MFC_CHANNEL_COUNT; checked++)
    {
        index = (p_context->next_channel + checked) % A_MFC_CHANNEL_COUNT;
        p_channel = &p_context->p_channels[index];
        if (0U == A_MFC_SelectRead(p_channel, now, &p_context->active_operation)) //选择往哪一路发送
        {
            continue;
        }
        p_context->next_channel = (index + 1U) % A_MFC_CHANNEL_COUNT;
        p_context->active_channel = index;
        result = A_MFC_StartRead(p_context->p_transaction, p_channel->address, p_context->active_operation, now);//发起ex201的数据查询
        if (A_EX201_RESULT_OK == result)
        {
            p_context->active = 1U;
        }
        else
        {
            A_MFC_Finish(p_context, result, now);
        }
        return;
    }
}

/*
 * 说明：获取MfcTask独占的通道状态指针
 * 输入：p_context 上下文，index 通道0～5
 * 输出：const A_MFC_Channel* 通道或NULL
 */
const A_MFC_Channel *A_MFC_GetChannel(const A_MFC_Context *p_context, uint32_t index)
{
    if ((NULL == p_context) || (0U == p_context->configured) || (index >= A_MFC_CHANNEL_COUNT))
    {
        return NULL;
    }
    return &p_context->p_channels[index];
}

/*
 * 说明：根据有效EX201应答及六路状态报告链路，尚不含下行CAN探测
 * 输入：p_context 上下文
 * 输出：uint32_t 0待确认，1RS485，3故障
 */
uint32_t A_MFC_GetLink(const A_MFC_Context *p_context)
{
    uint32_t index = 0U; // 当前检查的通道
    uint32_t probing = 0U; // 是否还有通道正在初始化
    if ((NULL == p_context) || (0U == p_context->configured))
    {
        return 0U;
    }
    if (0U == p_context->transport_ready)
    {
        return 3U;
    }
    for (index = 0U; index < A_MFC_CHANNEL_COUNT; index++)
    {
        if ((0U != p_context->p_channels[index].online) && (p_context->valid_responses >= 2U))
        {
            return 1U;
        }
        if (A_MFC_INIT_RUNNING == p_context->p_channels[index].initialize_state)
        {
            probing = 1U;
        }
    }
    return (0U != probing) ? 0U : 3U;
}

/*
 * 说明：接收由队列复制的写命令，轮询任务仍独占串口
 * 输入：p_context 上下文，p_command 命令
 * 输出：uint32_t 非0接收
 */
uint32_t A_MFC_SubmitCommand(A_MFC_Context *p_context, const A_MFC_Command *p_command)
{
    if ((NULL == p_context) || (NULL == p_command) ||
        (A_MFC_WRITE_IDLE != p_context->write_state))
    {
        return 0U;
    }
    p_context->write_command = *p_command;
    p_context->write_attempted = 0U;
    p_context->write_state = A_MFC_WRITE_PENDING;
    return 1U;
}

/*
 * 说明：保存业务结果，只有成功入结果队列后才释放请求槽
 * 输入：p_context 上下文，code 业务码，result EX201结果
 * 输出：无
 */
static void A_MFC_EndCommand(A_MFC_Context *p_context, A_MFC_CommandCode code, A_EX201_Result result)
{
    p_context->write_result.sequence = p_context->write_command.sequence;
    p_context->write_result.code = code;
    p_context->write_result.transport_result = result;
    p_context->write_state = A_MFC_WRITE_DONE;
}

/*
 * 说明：再次核对本通道元数据并转换流量，拒绝超出分辨率的目标
 * 输入：p_context 上下文
 * 输出：A_MFC_CommandCode 检查结果
 */
static A_MFC_CommandCode A_MFC_CheckTarget(A_MFC_Context *p_context)
{
    A_MFC_Channel *p_channel = &p_context->p_channels[p_context->write_command.index]; // 目标通道
    uint32_t required = A_EX201_DEVICE_INFO_FULL_SCALE_VALID | A_EX201_DEVICE_INFO_DECIMAL_VALID |
                        A_EX201_DEVICE_INFO_UNIT_VALID | A_EX201_DEVICE_INFO_SOURCE_VALID; // 必要元数据
    static const float s_scales[4] = {1.0F, 10.0F, 100.0F, 1000.0F}; // 尾数比例
    float target = p_context->write_command.target_flow; // 请求工程值
    float scaled = 0.0F; // 未取整尾数
    uint32_t rounded = 0U; // 最近的整数尾数
    if (!isfinite(target))
    {
        return A_MFC_COMMAND_VALUE;
    }
    if (target < 0.0F)
    {
        return A_MFC_COMMAND_RANGE;
    }
    if (!p_channel->online)
    {
        return A_MFC_COMMAND_OFFLINE;
    }
    if (!p_context->transport_ready || p_channel->initialize_state != A_MFC_INIT_READY ||
        (p_channel->device_info.valid_flags & required) != required ||
        p_channel->device_info.decimal_places > 3U ||
        p_channel->device_info.flow_source != A_EX201_FLOW_SOURCE_DIGITAL)
    {
        return A_MFC_COMMAND_NOT_READY;
    }
    if (target > (float) p_channel->device_info.full_scale_mantissa /
                 s_scales[p_channel->device_info.decimal_places])
    {
        return A_MFC_COMMAND_RANGE;
    }
    scaled = target * s_scales[p_channel->device_info.decimal_places];
    if (scaled > (float) EX201_FLOW_MANTISSA_MAX + 0.01F)
    {
        return A_MFC_COMMAND_RANGE;
    }
    rounded = (uint32_t) (scaled + 0.5F);
    if (rounded > p_channel->device_info.full_scale_mantissa)
    {
        return A_MFC_COMMAND_RANGE;
    }
    if (fabsf(scaled - (float) rounded) > 2.0F * FLT_EPSILON * fmaxf(1.0F, scaled))
    {
        return A_MFC_COMMAND_VALUE;
    }
    p_context->write_mantissa = (uint16_t) rounded;
    return A_MFC_COMMAND_OK;
}

/*
 * 说明：转换EX201事务错误，沿用轮询故障恢复但不重发写命令
 * 输入：p_context 上下文，result EX201错误，now 当前节拍
 * 输出：无
 */
static void A_MFC_CommandError(A_MFC_Context *p_context, A_EX201_Result result, TickType_t now)
{
    A_MFC_CommandCode code = A_MFC_COMMAND_PROTOCOL; // 错误分类
    if (result == A_EX201_RESULT_DEVICE_NG)
    {
        code = A_MFC_COMMAND_DEVICE_NG;
    }
    else if (result == A_EX201_RESULT_TX_TIMEOUT || result == A_EX201_RESULT_RESPONSE_TIMEOUT)
    {
        code = A_MFC_COMMAND_TIMEOUT;
    }
    else if (result == A_EX201_RESULT_SEND_ERROR || result == A_EX201_RESULT_RECEIVE_ERROR ||
             result == A_EX201_RESULT_RECOVERY_ERROR || result == A_EX201_RESULT_NOT_INITIALIZED)
    {
        code = A_MFC_COMMAND_DRIVER;
    }
    A_MFC_Finish(p_context, result, now);
    A_MFC_EndCommand(p_context, code, result);
}

/*
 * 说明：取消旧请求；尚未发送时不触碰轮询，已经发送的物理动作无法撤回
 * 输入：p_context 上下文，now 当前节拍
 * 输出：无
 */
static void A_MFC_CancelWrite(A_MFC_Context *p_context, TickType_t now)
{
    if (p_context->write_state == A_MFC_WRITE_SOURCE || p_context->write_state == A_MFC_WRITE_ACK ||
        p_context->write_state == A_MFC_WRITE_VERIFY)
    {
        if (A_EX201_RESULT_OK != A_EX201_Recover(p_context->p_transaction))
        {
            A_MFC_TransportFailed(p_context, now, A_EX201_RESULT_RECOVERY_ERROR);
        }
        p_context->guard_active = 1U;
        p_context->guard_tick = now;
    }
    A_MFC_EndCommand(p_context, A_MFC_COMMAND_EXPIRED, A_EX201_RESULT_NO_RESULT);
}

/*
 * 说明：推进RFSM确认、单次WSFD写入及RSFD读回，不阻塞其他RTOS任务
 * 输入：p_context 上下文，now 当前节拍，authorized 原CAN请求仍有效
 * 输出：无
 */
void A_MFC_ProcessCommand(A_MFC_Context *p_context, TickType_t now, uint32_t authorized)
{
    A_MFC_Channel *p_channel = NULL; // 目标通道
    A_EX201_Result result = A_EX201_RESULT_OK; // 当前事务结果
    A_MFC_CommandCode code = A_MFC_COMMAND_OK; // 参数检查结果
    int32_t mantissa = 0; // 读回尾数
    if (NULL == p_context || p_context->write_state == A_MFC_WRITE_IDLE ||
        p_context->write_state == A_MFC_WRITE_DONE)
    {
        return;
    }
    if (p_context->write_command.index >= A_MFC_CHANNEL_COUNT ||
        p_context->write_command.operation != A_MFC_COMMAND_SET_FLOW ||
        !p_context->write_command.sequence || !p_context->write_command.timeout_ticks ||
        p_context->write_command.timeout_ticks >= 0x80000000UL)
    {
        A_MFC_EndCommand(p_context, A_MFC_COMMAND_VALUE, A_EX201_RESULT_INVALID_ARGUMENT);
        return;
    }
    if (!authorized || (TickType_t) (now - p_context->write_command.started_tick) >=
                       p_context->write_command.timeout_ticks)
    {
        A_MFC_CancelWrite(p_context, now);
        return;
    }
    p_channel = &p_context->p_channels[p_context->write_command.index];
    // PENDING：等当前轮询结束，核对目标后发送RFSM，重新确认数字控制来源。
    if (p_context->write_state == A_MFC_WRITE_PENDING)
    {
        if (p_context->active) // 必须等当前轮询完整结束
        {
            return;
        }
        if (p_context->guard_active &&
            (TickType_t) (now - p_context->guard_tick) < A_MFC_ERROR_GUARD_MS)
        {
            return;
        }
        code = A_MFC_CheckTarget(p_context);
        if (code != A_MFC_COMMAND_OK)
        {
            A_MFC_EndCommand(p_context, code, A_EX201_RESULT_NO_RESULT);
            return;
        }
        p_context->active_channel = p_context->write_command.index;
        p_context->active_operation = A_MFC_READ_SOURCE;
        result = A_EX201_ReadFlowSource(p_context->p_transaction, p_channel->address, now);
        if (result == A_EX201_RESULT_OK)
        {
            p_context->write_state = A_MFC_WRITE_SOURCE;
        }
        else
        {
            A_MFC_CommandError(p_context, result, now);
        }
        return;
    }
    // START：最终检查通过后，只发送一次WSFD，不在这里等待仪器回复。
    if (p_context->write_state == A_MFC_WRITE_START)
    {
        code = A_MFC_CheckTarget(p_context);
        if (code != A_MFC_COMMAND_OK)
        {
            A_MFC_EndCommand(p_context, code, A_EX201_RESULT_NO_RESULT);
            return;
        }
        p_context->active_operation = A_MFC_READ_CONFIRMED;
        p_context->write_attempted = 1U;
        p_channel->flow_valid &= ~(A_MFC_VALID_CONFIRMED | A_MFC_VALID_TARGET);
        p_channel->revision++;
        result = A_EX201_SetFlow(p_context->p_transaction, p_channel->address, p_context->write_mantissa, now);
        if (result == A_EX201_RESULT_OK)
        {
            p_context->write_state = A_MFC_WRITE_ACK;
        }
        else
        {
            A_MFC_CommandError(p_context, result, now);
        }
        return;
    }
    // READBACK：WSFD返回OK后，还要发送RSFD读取仪器实际保存的设定值。
    if (p_context->write_state == A_MFC_WRITE_READBACK)
    {
        result = A_EX201_ReadSetFlow(p_context->p_transaction, p_channel->address, now);
        if (result == A_EX201_RESULT_OK)
        {
            p_context->write_state = A_MFC_WRITE_VERIFY;
        }
        else
        {
            A_MFC_CommandError(p_context, result, now);
        }
        return;
    }
    // SOURCE / ACK / VERIFY：只推进当前事务；没有完整响应就留到下一轮。
    A_EX201_Process(p_context->p_transaction, now);
    if (p_context->write_state == A_MFC_WRITE_SOURCE)
    {
        result = A_EX201_GetDeviceInfoResult(p_context->p_transaction, &p_channel->device_info);
    }
    else if (p_context->write_state == A_MFC_WRITE_ACK)
    {
        result = A_EX201_GetCommandResult(p_context->p_transaction);
    }
    else
    {
        result = A_EX201_GetFlowResult(p_context->p_transaction, &mantissa);
    }
    if (result == A_EX201_RESULT_BUSY)
    {
        return;
    }
    if (result != A_EX201_RESULT_OK)
    {
        A_MFC_CommandError(p_context, result, now);
        return;
    }
    A_MFC_Finish(p_context, A_EX201_RESULT_OK, now);
    if (p_context->write_state == A_MFC_WRITE_SOURCE)
    {
        p_context->write_state = A_MFC_WRITE_START;
    }
    else if (p_context->write_state == A_MFC_WRITE_ACK)
    {
        p_context->write_state = A_MFC_WRITE_READBACK;
    }
    else
    {
        p_channel->confirmed_mantissa = mantissa;
        p_channel->flow_valid |= A_MFC_VALID_CONFIRMED;
        if (mantissa == (int32_t) p_context->write_mantissa)
        {
            p_channel->target_mantissa = p_context->write_mantissa;
            p_channel->flow_valid |= A_MFC_VALID_TARGET;
            A_MFC_EndCommand(p_context, A_MFC_COMMAND_OK, result);
        }
        else
        {
            A_MFC_EndCommand(p_context, A_MFC_COMMAND_VERIFY, result);
        }
    }
}

/*
 * 说明：复制待入队的结果，读取操作不释放结果
 * 输入：p_context 上下文，p_result 输出结果
 * 输出：uint32_t 非0有结果
 */
uint32_t A_MFC_GetCommandResult(const A_MFC_Context *p_context, A_MFC_CommandResult *p_result)
{
    if (NULL == p_context || NULL == p_result || p_context->write_state != A_MFC_WRITE_DONE)
    {
        return 0U;
    }
    *p_result = p_context->write_result;
    return 1U;
}

/*
 * 说明：结果入队后释放槽，任何其他状态下调用均不丢弃命令
 * 输入：p_context 上下文
 * 输出：无
 */
void A_MFC_ReleaseCommand(A_MFC_Context *p_context)
{
    if (NULL != p_context && p_context->write_state == A_MFC_WRITE_DONE)
    {
        p_context->write_state = A_MFC_WRITE_IDLE;
    }
}
