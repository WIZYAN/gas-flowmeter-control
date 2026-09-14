/*
 * A_EX201.c
 *
 * Created on: 2026年9月14日
 * Author: YXZ
 */

#include "A_EX201.h"

#define A_EX201_TX_TIMEOUT_TICKS       pdMS_TO_TICKS(100U) // 9600 bit/s最大请求帧的发送超时初值
#define A_EX201_RESPONSE_TIMEOUT_TICKS pdMS_TO_TICKS(500U) // EX-201S响应超时初值，实板联调后调整

static const char s_set_flow_command[EX201_COMMAND_LENGTH] = {'W', 'S', 'F', 'D'};         // 设置数字流量尾数指令
static const char s_read_set_flow_command[EX201_COMMAND_LENGTH] = {'R', 'S', 'F', 'D'};    // 读取数字设定流量尾数指令
static const char s_read_actual_flow_command[EX201_COMMAND_LENGTH] = {'R', 'C', 'F', 'R'}; // 读取瞬时流量尾数指令
static const char s_close_flow_command[EX201_COMMAND_LENGTH] = {'W', 'V', 'S', 'S'};       // 设置数字阀门状态指令
static const uint8_t s_close_flow_data[] = {'2'};                                           // 数字阀门全关闭参数

/*
 * 说明：判断当前等待状态是否已经超时，减法写法允许系统节拍自然回绕
 * 输入：current_tick 当前系统节拍
 *      start_tick   等待开始节拍
 *      timeout_tick 允许等待的节拍数
 * 输出：uint32_t 非0表示已经超时，0表示尚未超时
 */
static uint32_t A_EX201_HasTimedOut(
    TickType_t current_tick,
    TickType_t start_tick,
    TickType_t timeout_tick)
{
    return (uint32_t) ((TickType_t) (current_tick - start_tick) >= timeout_tick);
}

/*
 * 说明：比较响应命令与当前请求命令是否一致
 * 输入：p_context        EX-201S事务上下文
 *      response_command 响应中的四字节命令
 * 输出：uint32_t 非0表示命令一致，0表示命令不一致
 */
static uint32_t A_EX201_IsCommandMatched(
    A_EX201_Context *p_context,
    const char response_command[EX201_COMMAND_LENGTH + 1U])
{
    size_t command_index = 0U; // 命令字符索引

    for (command_index = 0U; command_index < EX201_COMMAND_LENGTH; command_index++)
    {
        if (response_command[command_index] != p_context->expected_command[command_index])
        {
            return 0U;
        }
    }

    return 1U;
}

/*
 * 说明：记录事务错误并进入等待取走错误的状态
 * 输入：p_context EX-201S事务上下文
 *      result    需要记录的事务错误
 * 输出：无
 */
static void A_EX201_SetError(
    A_EX201_Context *p_context,
    A_EX201_Result result)
{
    p_context->result = result;
    p_context->state = A_EX201_STATE_ERROR;
}

/*
 * 说明：通过功能层恢复通信并记录原始错误或恢复错误
 * 输入：p_context EX-201S事务上下文
 *      result    通信恢复成功时需要记录的原始错误
 * 输出：无
 */
static void A_EX201_RecoverAndSetError(
    A_EX201_Context *p_context,
    A_EX201_Result result)
{
    F_EX201_TransportResult recover_result = F_EX201_TRANSPORT_RESULT_OK; // 功能层恢复结果

    recover_result = F_EX201_Recover(&p_context->function_context);

    if (F_EX201_TRANSPORT_RESULT_OK == recover_result)
    {
        A_EX201_SetError(p_context, result);
    }
    else
    {
        A_EX201_SetError(p_context, A_EX201_RESULT_RECOVERY_ERROR);
    }
}

/*
 * 说明：处理已经取出的完整EX-201S响应帧
 * 输入：p_context      EX-201S事务上下文
 *      response_length 完整响应帧长度
 * 输出：无
 */
static void A_EX201_HandleResponse(
    A_EX201_Context *p_context,
    size_t response_length)
{
    F_EX201_ProtocolResult protocol_result = F_EX201_PROTOCOL_RESULT_OK; // EX-201S协议解析结果

    protocol_result = F_EX201_DecodeResponse(
        p_context->response_frame,
        response_length,
        &p_context->response);

    if (F_EX201_PROTOCOL_RESULT_OK != protocol_result)
    {
        A_EX201_SetError(p_context, A_EX201_RESULT_PROTOCOL_ERROR);
        return;
    }

    if ((p_context->response.address != p_context->expected_address) ||
        (0U == A_EX201_IsCommandMatched(p_context, p_context->response.command)))
    {
        A_EX201_SetError(p_context, A_EX201_RESULT_RESPONSE_MISMATCH);
        return;
    }

    if (F_EX201_RESPONSE_STATUS_NG == p_context->response.status)
    {
        p_context->result = A_EX201_RESULT_DEVICE_NG;
    }
    else
    {
        p_context->result = A_EX201_RESULT_OK;
    }

    p_context->state = A_EX201_STATE_COMPLETE;
}

/*
 * 说明：初始化EX-201S事务模块及其下层模块
 * 输入：p_context EX-201S事务上下文
 * 输出：A_EX201_Result 初始化结果
 */
A_EX201_Result A_EX201_Initialize(A_EX201_Context *p_context)
{
    F_EX201_TransportResult initialize_result = F_EX201_TRANSPORT_RESULT_OK; // 功能层初始化结果
    size_t command_index = 0U;                                               // 命令字符索引

    if (p_context == NULL)
    {
        return A_EX201_RESULT_INVALID_ARGUMENT;
    }

    p_context->state = A_EX201_STATE_IDLE;
    p_context->result = A_EX201_RESULT_NO_RESULT;
    p_context->operation = A_EX201_OPERATION_NONE;
    p_context->state_start_tick = 0U;
    p_context->expected_address = 0U;
    p_context->request_sequence = 0U;
    p_context->initialized = 0U;

    for (command_index = 0U; command_index < EX201_COMMAND_LENGTH; command_index++)
    {
        p_context->expected_command[command_index] = '\0';
    }

    initialize_result = F_EX201_Initialize(&p_context->function_context);

    if (F_EX201_TRANSPORT_RESULT_OK != initialize_result)
    {
        p_context->state = A_EX201_STATE_ERROR;
        p_context->result = A_EX201_RESULT_INITIALIZE_ERROR;
        return A_EX201_RESULT_INITIALIZE_ERROR;
    }

    p_context->initialized = 1U;

    return A_EX201_RESULT_OK;
}

/*
 * 说明：启动一个非阻塞EX-201S请求事务
 * 输入：p_context    EX-201S事务上下文
 *      address      流量计通信地址
 *      command      四字节命令，不包含字符串结束符
 *      p_data       可见ASCII请求数据，data_length为0时允许为NULL
 *      data_length  请求数据长度
 *      current_tick 当前FreeRTOS系统节拍
 * 输出：A_EX201_Result 请求启动结果
 */
A_EX201_Result A_EX201_StartRequest(
    A_EX201_Context *p_context,//operation就是当前提交的任务
    uint16_t address,
    const char command[EX201_COMMAND_LENGTH],
    const uint8_t *p_data,
    size_t data_length,
    TickType_t current_tick)
{
    F_EX201_ProtocolResult protocol_result = F_EX201_PROTOCOL_RESULT_OK; // 请求帧编码结果
    F_EX201_TransportResult send_result = F_EX201_TRANSPORT_RESULT_OK;   // 请求帧发送启动结果
    size_t request_length = 0U;                                         // 请求帧实际长度
    size_t command_index = 0U;                                          // 命令字符索引

    if (p_context == NULL)
    {
        return A_EX201_RESULT_INVALID_ARGUMENT;
    }

    if (0U == p_context->initialized)
    {
        return A_EX201_RESULT_NOT_INITIALIZED;
    }

    if (A_EX201_STATE_IDLE != p_context->state)
    {
        return A_EX201_RESULT_BUSY;
    }

    protocol_result = F_EX201_EncodeRequest(
        address,
        command,
        p_data,
        data_length,
        p_context->request_frame,
        sizeof(p_context->request_frame),
        &request_length);

    if (F_EX201_PROTOCOL_RESULT_INVALID_ARGUMENT == protocol_result)
    {
        return A_EX201_RESULT_INVALID_ARGUMENT;
    }

    if (F_EX201_PROTOCOL_RESULT_OK != protocol_result)
    {
        return A_EX201_RESULT_PROTOCOL_ERROR;
    }

    send_result = F_EX201_SendFrame(
        &p_context->function_context,
        p_context->request_frame,
        request_length);

    if (F_EX201_TRANSPORT_RESULT_BUSY == send_result)
    {
        return A_EX201_RESULT_BUSY;
    }

    if (F_EX201_TRANSPORT_RESULT_OK != send_result)
    {
        return A_EX201_RESULT_SEND_ERROR;
    }

    p_context->expected_address = address;

    for (command_index = 0U; command_index < EX201_COMMAND_LENGTH; command_index++)
    {
        p_context->expected_command[command_index] = command[command_index];
    }

    p_context->request_sequence++;
    p_context->result = A_EX201_RESULT_NO_RESULT;
    p_context->operation = A_EX201_OPERATION_GENERIC;
    p_context->state_start_tick = current_tick;
    p_context->state = A_EX201_STATE_WAIT_TX_COMPLETE;

    return A_EX201_RESULT_OK;
}

/*
 * 说明：启动指定的EX-201S业务操作
 * 输入：p_context    EX-201S事务上下文
 *      address      流量计通信地址
 *      command      四字节EX-201S指令
 *      p_data       指令数据，data_length为0时允许为NULL
 *      data_length  指令数据长度
 *      current_tick 当前FreeRTOS系统节拍
 *      operation    业务操作类型
 * 输出：A_EX201_Result 请求启动结果
 */
static A_EX201_Result A_EX201_StartOperation(
    A_EX201_Context *p_context,
    uint16_t address,
    const char command[EX201_COMMAND_LENGTH],
    const uint8_t *p_data,
    size_t data_length,
    TickType_t current_tick,
    A_EX201_Operation operation)
{
    A_EX201_Result start_result = A_EX201_RESULT_OK; // 请求启动结果

    start_result = A_EX201_StartRequest(
        p_context,
        address,
        command,
        p_data,
        data_length,
        current_tick);

    if (A_EX201_RESULT_OK == start_result)
    {
        p_context->operation = operation;
    }

    return start_result;
}

/*
 * 说明：使用WSFD指令设置数字流量尾数
 * 输入：p_context     EX-201S事务上下文
 *      address       流量计通信地址
 *      flow_mantissa 四位流量尾数，且不得超过设备满刻度尾数
 *      current_tick  当前FreeRTOS系统节拍
 * 输出：A_EX201_Result 请求启动结果
 */
A_EX201_Result A_EX201_SetFlow(
    A_EX201_Context *p_context,
    uint16_t address,
    uint16_t flow_mantissa,
    TickType_t current_tick)
{
    uint8_t flow_data[EX201_FLOW_MANTISSA_LENGTH] = {0};           // WSFD四位流量尾数数据
    F_EX201_ProtocolResult encode_result = F_EX201_PROTOCOL_RESULT_OK; // 流量尾数编码结果

    encode_result = F_EX201_EncodeFlowValue(flow_mantissa, flow_data);

    if (F_EX201_PROTOCOL_RESULT_OK != encode_result)
    {
        return A_EX201_RESULT_INVALID_ARGUMENT;
    }

    return A_EX201_StartOperation(
        p_context,
        address,
        s_set_flow_command,
        flow_data,
        sizeof(flow_data),
        current_tick,
        A_EX201_OPERATION_SET_FLOW);
}

/*
 * 说明：使用RSFD指令读取数字设定流量尾数
 * 输入：p_context    EX-201S事务上下文
 *      address      流量计通信地址
 *      current_tick 当前FreeRTOS系统节拍
 * 输出：A_EX201_Result 请求启动结果
 */
A_EX201_Result A_EX201_ReadSetFlow(
    A_EX201_Context *p_context,
    uint16_t address,
    TickType_t current_tick)
{
    return A_EX201_StartOperation(
        p_context,
        address,
        s_read_set_flow_command,
        NULL,
        0U,
        current_tick,
        A_EX201_OPERATION_READ_SET_FLOW);
}

/*
 * 说明：使用RCFR指令读取带符号的瞬时流量尾数
 * 输入：p_context    EX-201S事务上下文
 *      address      流量计通信地址
 *      current_tick 当前FreeRTOS系统节拍
 * 输出：A_EX201_Result 请求启动结果
 */
A_EX201_Result A_EX201_ReadActualFlow(
    A_EX201_Context *p_context,
    uint16_t address,
    TickType_t current_tick)
{
    return A_EX201_StartOperation(
        p_context,
        address,
        s_read_actual_flow_command,
        NULL,
        0U,
        current_tick,
        A_EX201_OPERATION_READ_ACTUAL_FLOW);
}

/*
 * 说明：使用WVSS指令请求数字阀门全关闭
 * 输入：p_context    EX-201S事务上下文
 *      address      流量计通信地址
 *      current_tick 当前FreeRTOS系统节拍
 * 输出：A_EX201_Result 请求启动结果
 */
A_EX201_Result A_EX201_CloseFlow(
    A_EX201_Context *p_context,
    uint16_t address,
    TickType_t current_tick)
{
    return A_EX201_StartOperation(
        p_context,
        address,
        s_close_flow_command,
        s_close_flow_data,
        sizeof(s_close_flow_data),
        current_tick,
        A_EX201_OPERATION_CLOSE_FLOW);
}

/*
 * 说明：推进EX-201S非阻塞事务状态机，必须由MfcTask周期调用
 * 输入：p_context    EX-201S事务上下文
 *      current_tick 当前FreeRTOS系统节拍
 * 输出：无
 */
void A_EX201_Process(
    A_EX201_Context *p_context,
    TickType_t current_tick)
{
    F_EX201_TransportResult receive_result = F_EX201_TRANSPORT_RESULT_OK; // 功能层接收结果
    size_t response_length = 0U;                                         // 完整响应帧长度

    if ((p_context == NULL) || (0U == p_context->initialized))
    {
        return;
    }

    if (A_EX201_STATE_WAIT_TX_COMPLETE == p_context->state)
    {
        if (0U == F_EX201_IsTransmitBusy(&p_context->function_context))
        {
            p_context->state_start_tick = current_tick;
            p_context->state = A_EX201_STATE_WAIT_RESPONSE;
        }
        else if (0U != A_EX201_HasTimedOut(
                          current_tick,
                          p_context->state_start_tick,
                          A_EX201_TX_TIMEOUT_TICKS))
        {
            A_EX201_RecoverAndSetError(p_context, A_EX201_RESULT_TX_TIMEOUT);
        }

        return;
    }

    if (A_EX201_STATE_WAIT_RESPONSE != p_context->state)
    {
        return;
    }

    receive_result = F_EX201_ReceiveFrame(
        &p_context->function_context,
        p_context->response_frame,
        sizeof(p_context->response_frame),
        &response_length);

    if (F_EX201_TRANSPORT_RESULT_OK == receive_result)
    {
        A_EX201_HandleResponse(p_context, response_length);
        return;
    }

    if (F_EX201_TRANSPORT_RESULT_NO_FRAME != receive_result)
    {
        A_EX201_RecoverAndSetError(p_context, A_EX201_RESULT_RECEIVE_ERROR);
        return;
    }

    if (0U != A_EX201_HasTimedOut(
                  current_tick,
                  p_context->state_start_tick,
                  A_EX201_RESPONSE_TIMEOUT_TICKS))
    {
        A_EX201_RecoverAndSetError(p_context, A_EX201_RESULT_RESPONSE_TIMEOUT);
    }
}

/*
 * 说明：查询事务或尚未取走的结果是否占用客户端
 * 输入：p_context EX-201S事务上下文
 * 输出：uint32_t 非0表示不能启动新事务，0表示空闲
 */
uint32_t A_EX201_IsBusy(A_EX201_Context *p_context)
{
    if ((p_context == NULL) || (0U == p_context->initialized))
    {
        return 0U;
    }

    return (uint32_t) (A_EX201_STATE_IDLE != p_context->state);
}

/*
 * 说明：读取已完成事务的结果并使客户端恢复空闲
 * 输入：p_context  EX-201S事务上下文
 *      p_response 成功或设备返回NG时的响应数据；普通错误时允许为NULL
 * 输出：A_EX201_Result 事务执行结果、忙状态或无结果状态
 */
A_EX201_Result A_EX201_GetResult(
    A_EX201_Context *p_context,
    F_EX201_Response *p_response)
{
    A_EX201_Result result = A_EX201_RESULT_NO_RESULT; // 返回给调用者的事务结果

    if (p_context == NULL)
    {
        return A_EX201_RESULT_INVALID_ARGUMENT;
    }

    if (0U == p_context->initialized)
    {
        return A_EX201_RESULT_NOT_INITIALIZED;
    }

    result = p_context->result;

    if (A_EX201_STATE_IDLE == p_context->state)
    {
        return A_EX201_RESULT_NO_RESULT;
    }

    if ((A_EX201_STATE_WAIT_TX_COMPLETE == p_context->state) ||
        (A_EX201_STATE_WAIT_RESPONSE == p_context->state))
    {
        return A_EX201_RESULT_BUSY;
    }

    if (A_EX201_STATE_COMPLETE == p_context->state)
    {
        if (p_response == NULL)
        {
            return A_EX201_RESULT_INVALID_ARGUMENT;
        }

        *p_response = p_context->response;
    }

    p_context->state = A_EX201_STATE_IDLE;
    p_context->result = A_EX201_RESULT_NO_RESULT;
    p_context->operation = A_EX201_OPERATION_NONE;

    return result;
}

/*
 * 说明：读取RSFD或RCFR业务操作返回的流量尾数并恢复空闲
 * 输入：p_context       EX-201S事务上下文
 *      p_flow_mantissa 输出的有符号流量尾数
 * 输出：A_EX201_Result 事务及流量数据解析结果
 */
A_EX201_Result A_EX201_GetFlowResult(
    A_EX201_Context *p_context,
    int32_t *p_flow_mantissa)
{
    F_EX201_Response g_response = {0};                             // 当前EX-201S响应
    A_EX201_Result transaction_result = A_EX201_RESULT_NO_RESULT; // 当前事务结果
    F_EX201_ProtocolResult decode_result = F_EX201_PROTOCOL_RESULT_OK; // 流量数据解析结果

    if ((p_context == NULL) || (p_flow_mantissa == NULL))
    {
        return A_EX201_RESULT_INVALID_ARGUMENT;
    }

    if (0U == p_context->initialized)
    {
        return A_EX201_RESULT_NOT_INITIALIZED;
    }

    if ((A_EX201_OPERATION_READ_SET_FLOW != p_context->operation) &&
        (A_EX201_OPERATION_READ_ACTUAL_FLOW != p_context->operation))
    {
        return A_EX201_RESULT_INVALID_ARGUMENT;
    }

    transaction_result = A_EX201_GetResult(p_context, &g_response);

    if (A_EX201_RESULT_OK != transaction_result)
    {
        return transaction_result;
    }

    decode_result = F_EX201_DecodeFlowValue(
        g_response.data,
        (size_t) g_response.data_length,
        p_flow_mantissa);

    if (F_EX201_PROTOCOL_RESULT_OK != decode_result)
    {
        return A_EX201_RESULT_PROTOCOL_ERROR;
    }

    return A_EX201_RESULT_OK;
}

/*
 * 说明：读取WSFD或WVSS业务操作的执行结果并恢复空闲
 * 输入：p_context EX-201S事务上下文
 * 输出：A_EX201_Result 事务执行结果
 */
A_EX201_Result A_EX201_GetCommandResult(A_EX201_Context *p_context)
{
    F_EX201_Response g_response = {0};                             // 当前EX-201S响应
    A_EX201_Result transaction_result = A_EX201_RESULT_NO_RESULT; // 当前事务结果

    if (p_context == NULL)
    {
        return A_EX201_RESULT_INVALID_ARGUMENT;
    }

    if (0U == p_context->initialized)
    {
        return A_EX201_RESULT_NOT_INITIALIZED;
    }

    if ((A_EX201_OPERATION_SET_FLOW != p_context->operation) &&
        (A_EX201_OPERATION_CLOSE_FLOW != p_context->operation))
    {
        return A_EX201_RESULT_INVALID_ARGUMENT;
    }

    transaction_result = A_EX201_GetResult(p_context, &g_response);

    if (A_EX201_RESULT_OK != transaction_result)
    {
        return transaction_result;
    }

    if (0U != g_response.data_length)
    {
        return A_EX201_RESULT_PROTOCOL_ERROR;
    }

    return A_EX201_RESULT_OK;
}
