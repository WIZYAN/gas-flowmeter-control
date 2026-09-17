/*
 * Created on: 2026年9月15日
 * Author: CI
 */
#ifndef SRC_A_CONTROL_H_
#define SRC_A_CONTROL_H_
#include "A_MFC.h"
#include "A_HostCan.h"
#include "queue.h"

#define A_CONTROL_HOST_STATE_MAX_AGE_MS (20U) // CAN任务有效状态的最长使用时间，超期停止写事务

typedef struct
{
    uint32_t sequence; // 当前CAN请求号，0表示无有效请求
    TickType_t started_tick; // 原请求起点，不能在转发时重置总期限
    TickType_t updated_tick; // CAN任务最近一次发布状态的时间
    uint32_t valid; // 原请求仍有效
} A_Control_Host_State;

typedef struct
{
    uint32_t sequence; // 原CAN请求号
    uint32_t code; // 已映射的CAN_USER业务结果码
} A_Control_Host_Result;

typedef struct
{
    QueueHandle_t host_command; // CAN到Control的业务命令
    QueueHandle_t host_result; // Control到CAN的执行结果
    QueueHandle_t command; // Control到MFC的执行命令
    QueueHandle_t result; // MFC到Control的执行结果
    QueueHandle_t host_state; // CAN到Control的最新请求有效状态
} A_Control_Queue_Set;

typedef struct
{
    A_Control_Queue_Set queues; // 启动后只读的队列句柄
    A_Control_Host_State host_state; // 本任务出队取得的CAN状态副本
    A_Control_Host_Result pending_result; // 回复队列满时保留的结果
    uint32_t result_pending; // 结果尚未送入CAN结果队列
    uint32_t active_sequence; // 等待MfcTask结束的请求号，0表示空闲
    uint32_t initialized; // 五个队列已接入
} A_Control_Context;

/*
 * 说明：绑定静态队列，只由ControlTask在启动阶段调用
 * 输入：p_context 零初始化上下文，p_queues 五个有效队列句柄
 * 输出：uint32_t 非0成功
 */
uint32_t A_Control_Initialize(A_Control_Context *p_context, const A_Control_Queue_Set *p_queues);
/*
 * 说明：领取CAN流量写请求并通过队列交接，结果由HostCanStack发送
 * 输入：p_context 本任务独占上下文，now 当前1ms节拍
 * 输出：无
 */
void A_Control_Process(A_Control_Context *p_context, TickType_t now);
/*
 * 说明：仅检查本任务出队的状态副本，不访问其他任务上下文
 * 输入：p_state CAN状态副本，sequence 待执行请求号，started_tick 请求起点，now 当前节拍
 * 输出：uint32_t 非0表示请求匹配、状态新鲜且未超过总期限
 */
uint32_t A_Control_RequestValid(const A_Control_Host_State *p_state, uint32_t sequence,
                               TickType_t started_tick, TickType_t now);
#endif
