/*
 * Created on: 2026年9月15日
 * Author: CI
 */
#include "A_System.h"
#include "task.h"

#if A_MFC_CHANNEL_COUNT != A_HOSTCAN_CHANNEL_COUNT
#error "MFC and host CAN channel counts must match."
#endif

// CAN任务：业务状态和驱动状态各自存放，不把驱动结构一层层嵌进业务结构。
static F_HostCan_Context g_can_transport = {0}; // CAN0驱动及中断缓冲
static A_HostCan_Context g_host_can = {.p_transport = &g_can_transport}; // CAN任务状态

// MFC任务：六路通道、唯一事务、组帧缓冲各自存放，地址在启动前固定。
static F_EX201_Context g_ex201_function = {0}; // EX201组帧与RS485硬件状态
static A_EX201_Context g_ex201_transaction = {.p_function = &g_ex201_function}; // 单个EX201事务
static A_MFC_Channel g_mfc_channels[A_MFC_CHANNEL_COUNT] = {0}; // 六路参数和采集状态
static A_MFC_Context g_mfc = {
    .p_transaction = &g_ex201_transaction,
    .p_channels = g_mfc_channels
}; // 六路轮询及写入调度状态

static A_Control_Context g_control = {0}; // Control任务状态
static A_System_Telemetry g_mfc_telemetry = {0}; // MFC任务的队列发送副本
static A_System_Telemetry g_host_telemetry = {0}; // CAN任务的队列接收副本

// 这里只登记固定地址和队列资源；任务间业务数据仍必须通过七个队列复制。
static A_System_Context g_system = {
    .p_host_can = &g_host_can,
    .p_mfc = &g_mfc,
    .p_control = &g_control,
    .p_mfc_telemetry = &g_mfc_telemetry,
    .p_host_telemetry = &g_host_telemetry
}; // 启动资源入口，不再嵌套全部任务数据

/*
 * 说明：返回任务共用的长期上下文，不复制串口或CAN控制状态
 * 输入：无
 * 输出：A_System_Context* 板级上下文
 */
A_System_Context *A_System_GetContext(void)
{
    return &g_system;
}

/*
 * 说明：按本通道小数位换算工程值并显式映射有效标志
 * 输入：p_source MfcTask独占通道，p_target 输出CAN参数快照
 * 输出：无
 */
static void A_System_MapChannel(const A_MFC_Channel *p_source, A_HostCan_Channel *p_target)
{
    static const float s_divisors[4] = {1.0F, 10.0F, 100.0F, 1000.0F}; // 小数位比例
    uint32_t info_flags = p_source->device_info.valid_flags; // EX201参数有效位
    float divisor = 1.0F; // 本通道换算除数
    p_target->device_address = p_source->address;
    p_target->valid_flags = A_HOSTCAN_VALID_ADDRESS;
    p_target->online = p_source->online;
    p_target->initialize_state = (uint32_t) p_source->initialize_state;
    p_target->sampled_ms = (uint32_t) p_source->sampled_tick;
    p_target->decimal_places = p_source->device_info.decimal_places;
    p_target->unit = (uint32_t) p_source->device_info.flow_unit;
    p_target->flow_source = (uint32_t) p_source->device_info.flow_source;
    p_target->internal_valve = (uint32_t) p_source->device_info.valve_state;
    p_target->alarm = p_source->device_info.alarm_state;
    if (0U != (info_flags & A_EX201_DEVICE_INFO_UNIT_VALID))
    {
        p_target->valid_flags |= A_HOSTCAN_VALID_UNIT;
    }
    if (0U != (info_flags & A_EX201_DEVICE_INFO_SOURCE_VALID))
    {
        p_target->valid_flags |= A_HOSTCAN_VALID_SOURCE;
    }
    if (0U != (info_flags & A_EX201_DEVICE_INFO_VALVE_STATE_VALID))
    {
        p_target->valid_flags |= A_HOSTCAN_VALID_VALVE;
    }
    if (0U != (info_flags & A_EX201_DEVICE_INFO_ALARM_VALID))
    {
        p_target->valid_flags |= A_HOSTCAN_VALID_ALARM;
    }
    if ((0U != (info_flags & A_EX201_DEVICE_INFO_DECIMAL_VALID)) && (p_target->decimal_places <= 3U))
    {
        p_target->valid_flags |= A_HOSTCAN_VALID_DECIMAL;
        divisor = s_divisors[p_target->decimal_places];
        if (0U != (info_flags & A_EX201_DEVICE_INFO_UNIT_VALID))
        {
            if (0U != (info_flags & A_EX201_DEVICE_INFO_FULL_SCALE_VALID))
            {
                p_target->full_scale = (float) p_source->device_info.full_scale_mantissa / divisor;
                p_target->valid_flags |= A_HOSTCAN_VALID_SCALE;
            }
            if (0U != (p_source->flow_valid & A_MFC_VALID_ACTUAL))
            {
                p_target->actual_flow = (float) p_source->actual_mantissa / divisor;
                p_target->valid_flags |= A_HOSTCAN_VALID_ACTUAL;
            }
            if (0U != (p_source->flow_valid & A_MFC_VALID_CONFIRMED))
            {
                p_target->confirmed_flow = (float) p_source->confirmed_mantissa / divisor;
                p_target->valid_flags |= A_HOSTCAN_VALID_CONFIRMED;
            }
            if (0U != (p_source->flow_valid & A_MFC_VALID_TARGET))
            {
                p_target->target_flow = (float) p_source->target_mantissa / divisor;
                p_target->valid_flags |= A_HOSTCAN_VALID_TARGET;
            }
        }
    }
}

/*
 * 说明：启动时集中创建七个静态队列；共享的仅为初始化配置和之后不变的句柄
 * 输入：p_context 板级上下文
 * 输出：uint32_t 非0表示全部队列可用
 */
uint32_t A_System_Initialize(A_System_Context *p_context)
{
    uint32_t ready = 0U; // 本次初始化结果
    if (NULL == p_context)
    {
        return 0U;
    }
    taskENTER_CRITICAL();
    if (!p_context->queues_ready)
    {
        // Control任务 → MFC任务：执行命令。
        if (p_context->command_queue == NULL)
        {
            p_context->command_queue = xQueueCreateStatic(
                1U,
                sizeof(A_MFC_Command),
                p_context->command_storage,
                &p_context->command_queue_memory);
        }

        // MFC任务 → Control任务：实际执行结果。
        if (p_context->result_queue == NULL)
        {
            p_context->result_queue = xQueueCreateStatic(
                1U,
                sizeof(A_MFC_CommandResult),
                p_context->result_storage,
                &p_context->result_queue_memory);
        }

        // CAN任务 → Control任务：已解析的上位机写请求。
        if (p_context->host_command_queue == NULL)
        {
            p_context->host_command_queue = xQueueCreateStatic(
                1U,
                sizeof(A_HostCan_Command),
                p_context->host_command_storage,
                &p_context->host_command_queue_memory);
        }

        // Control任务 → CAN任务：写入执行结果，不是周期查询数据。
        if (p_context->host_result_queue == NULL)
        {
            p_context->host_result_queue = xQueueCreateStatic(
                1U,
                sizeof(A_Control_Host_Result),
                p_context->host_result_storage,
                &p_context->host_result_queue_memory);
        }

        // MFC任务 → CAN任务：六路完整最新数据。
        if (p_context->telemetry_queue == NULL)
        {
            p_context->telemetry_queue = xQueueCreateStatic(
                1U,
                sizeof(A_System_Telemetry),
                p_context->telemetry_storage,
                &p_context->telemetry_queue_memory);
        }

        // CAN任务 → Control任务：请求是否仍有效。
        if (p_context->control_state_queue == NULL)
        {
            p_context->control_state_queue = xQueueCreateStatic(
                1U,
                sizeof(A_Control_Host_State),
                p_context->control_state_storage,
                &p_context->control_state_queue_memory);
        }

        // CAN任务 → MFC任务：独立消费的请求有效状态。
        if (p_context->mfc_state_queue == NULL)
        {
            p_context->mfc_state_queue = xQueueCreateStatic(
                1U,
                sizeof(A_Control_Host_State),
                p_context->mfc_state_storage,
                &p_context->mfc_state_queue_memory);
        }

        p_context->queues_ready = (p_context->command_queue && p_context->result_queue &&
            p_context->host_command_queue && p_context->host_result_queue && p_context->telemetry_queue &&
            p_context->control_state_queue && p_context->mfc_state_queue) ? 1U : 0U;
    }
    ready = p_context->queues_ready;
    taskEXIT_CRITICAL();
    return ready;
}

/*
 * 说明：CAN任务独占接收快照和CAN缓存，其他任务仅向telemetry_queue写副本
 * 输入：p_context 板级上下文
 * 输出：无
 */
static void A_System_ReceiveTelemetry(A_System_Context *p_context)
{
    A_HostCan_Context *p_host = p_context->p_host_can; // 本函数只在CAN任务运行
    A_System_Telemetry *p_snapshot = p_context->p_host_telemetry; // CAN任务接收副本
    uint32_t index = 0U; // 通道索引
    if (pdPASS != xQueueReceive(p_context->telemetry_queue, p_snapshot, 0U))//传感器的循环数据队列循环，给主机can，使用recieve之后会移除数据。//把六路数据放在p_snapshot
    {
        return;
    }
    for (index = 0U; index < A_HOSTCAN_CHANNEL_COUNT; index++)
    {
        (void) A_HostCan_PublishChannel(p_host, index, &p_snapshot->channels[index]);
    }
    (void) A_HostCan_PublishMfcLink(p_host, p_snapshot->link);
    A_HostCan_SetExecutors(p_host,
        p_snapshot->ready ? A_HOSTCAN_EXECUTOR_MFC : 0U);
}

/*
 * 说明：仅在CAN任务访问host_can；任务间命令、结果、快照及有效状态都按值入队
 * 输入：p_context 板级上下文，now 当前节拍
 * 输出：无
 */
void A_System_ProcessHostCan(A_System_Context *p_context, TickType_t now)
{
    A_HostCan_Context *p_host = NULL; // 只访问CAN任务自己的业务状态
    A_HostCan_Command g_command = {0}; // CAN任务独占的出队发送副本
    A_Control_Host_Result g_result = {0}; // 从Control收到的结果
    A_Control_Host_State g_state = {0}; // 发给两个消费者的相同状态，各自使用独立队列
    if (NULL == p_context || NULL == p_context->p_host_can || NULL == p_context->p_host_telemetry)
    {
        return;
    }
    p_host = p_context->p_host_can;
    if (!p_host->initialized)
    {
        return;
    }
    if (!A_System_Initialize(p_context))
    {
        A_HostCan_Process(p_host, now); // 未接入执行器时仍可查询版本并拒绝写请求
        return;
    }
    // 第1步：先从MFC遥测队列更新CAN自己的参数缓存。
    A_System_ReceiveTelemetry(p_context);

    // 第2步：接收Control转来的实际执行结果，核对请求号后交给CAN协议处理。
    if (pdPASS == xQueueReceive(p_context->host_result_queue, &g_result, 0U))
    {
        A_System_ReceiveTelemetry(p_context); // MFC可能在上次出队后抢占并发布结果，先取结果对应的新快照
        (void) A_HostCan_CompleteCommand(p_host, g_result.sequence, (uint8_t) g_result.code, now);
    }
    // 第3步：推进CAN收发，解析上位机查询和写请求。
    A_HostCan_Process(p_host, now);

    // 第4步：提取本任务刚接受的写请求，按值送给Control任务。
    taskENTER_CRITICAL(); // 发布请求与有效状态不可被Control/MFC插入，硬件等待不在临界区中
    if (A_HostCan_TakeCommand(p_host, &g_command))
    {
        if (pdPASS != xQueueSendToBack(p_context->host_command_queue, &g_command, 0U))//将数据发送到队列尾部，队列信息送入控制任务control_task
        {
            (void) A_HostCan_CompleteCommand(p_host, g_command.sequence, A_HOSTCAN_CODE_BUSY, now);
        }
    }
    // 第5步：两个消费者各有一份队列消息；一个任务出队不会取走另一个任务的消息。
    g_state.sequence = p_host->command.sequence;
    g_state.started_tick = p_host->command.started_ms;
    g_state.updated_tick = now;
    g_state.valid = A_HostCan_CommandActive(p_host, g_state.sequence, now);
    (void) xQueueOverwrite(p_context->control_state_queue, &g_state);//将控制状态写入
    (void) xQueueOverwrite(p_context->mfc_state_queue, &g_state);
    taskEXIT_CRITICAL();
}

/*
 * 说明：MFC任务读取命令及CAN有效状态队列，推进事务并覆盖发布六路完整快照
 * 输入：p_context 板级上下文，now 当前节拍
 * 输出：无
 */
void A_System_ProcessMfc(A_System_Context *p_context, TickType_t now)
{
    A_MFC_Context *p_mfc = NULL; // 本任务的六路调度状态
    A_System_Telemetry *p_snapshot = NULL; // 本任务的完整发送快照
    uint32_t index = 0U; // 当前通道索引0～5
    uint32_t changed = 0U; // 本轮是否有通道数据变化
    uint32_t link = 0U; // 当前下行链路状态
    uint32_t ready = 0U; // MFC调度入口是否配置完成
    const A_MFC_Channel *p_channel = NULL; // 仅本任务访问采集上下文
    A_MFC_Command g_command = {0}; // 命令队列副本
    A_MFC_CommandResult g_result = {0}; // 执行结果副本
    uint32_t authorized = 0U; // 原CAN请求在本轮是否仍有执行资格
    uint32_t queues_ready = 0U; // 七个静态队列是否都已创建
    if (NULL == p_context || NULL == p_context->p_mfc || NULL == p_context->p_mfc_telemetry)
    {
        return;
    }
    p_mfc = p_context->p_mfc;
    p_snapshot = p_context->p_mfc_telemetry;
    queues_ready = A_System_Initialize(p_context);
    // 第1步：队列可用且设备地址配置完成后，才允许接收写命令。
    if (queues_ready && p_mfc->configured)
    {
        if (p_mfc->write_state == A_MFC_WRITE_IDLE &&
            pdPASS == xQueueReceive(p_context->command_queue, &g_command, 0U))
        {
            (void) A_MFC_SubmitCommand(p_mfc, &g_command);
        }
        // 第2步：取CAN有效状态并检查。只有START阶段需要把检查与发送启动连在一起。
        taskENTER_CRITICAL(); // 保证最后一次状态出队检查与WSFD非阻塞启动连续执行
        (void) xQueueReceive(p_context->mfc_state_queue, &p_context->mfc_host_state, 0U);
        authorized = A_Control_RequestValid(&p_context->mfc_host_state,
            p_mfc->write_command.sequence, p_mfc->write_command.started_tick, now);
        if (p_mfc->write_state == A_MFC_WRITE_START)
        {
            A_MFC_ProcessCommand(p_mfc, now, authorized);
            taskEXIT_CRITICAL();
        }
        else
        {
            taskEXIT_CRITICAL();
            A_MFC_ProcessCommand(p_mfc, now, authorized);
        }
    }
    // 第3步：推进六路轮询。写事务正在占用串口时，A_MFC_Process内部会让出总线。
    A_MFC_Process(p_mfc, now); // 循环读取六路数据，不依赖CAN任务是否运行
    if (!queues_ready)
    {
        return;
    }
    // 第4步：只转换发生变化的通道；发送副本中始终保留全部六路。
    for (index = 0U; index < A_MFC_CHANNEL_COUNT; index++)
    {
        p_channel = A_MFC_GetChannel(p_mfc, index);
        if (NULL == p_channel || p_context->published_revision[index] == p_channel->revision)
        {
            continue;
        }
        p_snapshot->channels[index] = (A_HostCan_Channel) {0};
        A_System_MapChannel(p_channel, &p_snapshot->channels[index]); // 按各路比例形成工程值
        p_context->published_revision[index] = p_channel->revision;
        changed = 1U;
    }
    link = A_MFC_GetLink(p_mfc);
    ready = p_mfc->configured;
    // 第5步：新数据覆盖旧快照，CAN任务下次出队时取得最新完整数据。
    if (changed || link != p_snapshot->link || ready != p_snapshot->ready)
    {
        p_snapshot->link = link;
        p_snapshot->ready = ready;
        (void) xQueueOverwrite(p_context->telemetry_queue, p_snapshot);
    }
    // 第6步：先发布确认数据，再发送执行结果；结果队列满时保留DONE状态。
    if (A_MFC_GetCommandResult(p_mfc, &g_result) &&
        pdPASS == xQueueSendToBack(p_context->result_queue, &g_result, 0U))
    {
        A_MFC_ReleaseCommand(p_mfc);
    }
}

/*
 * 说明：Control任务只绑定队列并处理自己的上下文，不读取CAN和MFC业务内存
 * 输入：p_context 板级上下文，now 当前节拍
 * 输出：无
 */
void A_System_ProcessControl(A_System_Context *p_context, TickType_t now)
{
    A_Control_Queue_Set g_queues = {0}; // 仅复制启动后不变的队列句柄
    if (NULL == p_context || !A_System_Initialize(p_context))
    {
        return;
    }
    g_queues.host_command = p_context->host_command_queue;
    g_queues.host_result = p_context->host_result_queue;
    g_queues.command = p_context->command_queue;//controltask to mfctask
    g_queues.result = p_context->result_queue;
    g_queues.host_state = p_context->control_state_queue;//各个队列数据进行统一管理
    if (!A_Control_Initialize(p_context->p_control, &g_queues))
    {
        return;
    }
    A_Control_Process(p_context->p_control, now);
}
