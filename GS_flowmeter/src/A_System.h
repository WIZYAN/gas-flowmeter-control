/*
 * Created on: 2026年9月15日
 * Author: CI
 */
#ifndef SRC_A_SYSTEM_H_
#define SRC_A_SYSTEM_H_
#include "A_MFC.h"
#include "A_HostCan.h"
#include "A_Control.h"

#define A_SYSTEM_MFC_QUEUE_LENGTH (1U) // 与单笔未完成写请求相匹配

typedef struct
{
    A_HostCan_Channel channels[A_HOSTCAN_CHANNEL_COUNT]; // 同一条消息包含六路最新数据，不丢失其他路
    uint32_t link; // 下行链路状态
    uint32_t ready; // MFC执行入口已接入，不代表每台仪器都已就绪
} A_System_Telemetry;

typedef struct
{
    A_HostCan_Context *p_host_can; // CAN任务的独立状态地址，仅CAN任务使用
    A_MFC_Context *p_mfc;          // MFC任务的独立状态地址，仅MFC任务使用
    uint32_t published_revision[A_MFC_CHANNEL_COUNT]; // MfcTask保存已打包的通道版本
    A_Control_Context *p_control; // Control任务的独立状态地址，仅Control任务使用
    A_System_Telemetry *p_mfc_telemetry; // MFC任务独占的完整发送快照
    A_System_Telemetry *p_host_telemetry; // CAN任务独占的完整接收快照
    A_Control_Host_State mfc_host_state; // MfcTask出队后持有的请求状态副本
    StaticQueue_t command_queue_memory; // 命令队列控制块，无动态内存
    StaticQueue_t result_queue_memory; // 结果队列控制块
    StaticQueue_t host_command_queue_memory; // CAN到Control队列控制块
    StaticQueue_t host_result_queue_memory; // Control到CAN队列控制块
    StaticQueue_t telemetry_queue_memory; // MFC到CAN六路覆盖队列控制块
    StaticQueue_t control_state_queue_memory; // CAN到Control状态覆盖队列控制块
    StaticQueue_t mfc_state_queue_memory; // CAN到MFC状态覆盖队列控制块
    uint8_t command_storage[A_SYSTEM_MFC_QUEUE_LENGTH * sizeof(A_MFC_Command)]; // 按值复制命令
    uint8_t result_storage[A_SYSTEM_MFC_QUEUE_LENGTH * sizeof(A_MFC_CommandResult)]; // 按值复制结果
    uint8_t host_command_storage[sizeof(A_HostCan_Command)]; // 单笔CAN业务命令
    uint8_t host_result_storage[sizeof(A_Control_Host_Result)]; // 单笔CAN业务结果
    uint8_t telemetry_storage[sizeof(A_System_Telemetry)]; // 六路最新快照的队列副本
    uint8_t control_state_storage[sizeof(A_Control_Host_State)]; // Control独立消费，不能与MFC竞争出队
    uint8_t mfc_state_storage[sizeof(A_Control_Host_State)]; // MFC独立消费的状态
    QueueHandle_t command_queue; // ControlTask生产，MfcTask消费
    QueueHandle_t result_queue; // MfcTask生产，ControlTask消费
    QueueHandle_t host_command_queue; // HostCanStack生产，ControlTask消费
    QueueHandle_t host_result_queue; // ControlTask生产，HostCanStack消费
    QueueHandle_t telemetry_queue; // MfcTask覆盖写，HostCanStack消费
    QueueHandle_t control_state_queue; // HostCanStack覆盖写，ControlTask消费
    QueueHandle_t mfc_state_queue; // HostCanStack覆盖写，MfcTask消费
    uint32_t queues_ready; // 仅启动配置同步，创建完成后不再修改；业务状态一律走队列
} A_System_Context;

/*
 * 说明：取得固定资源入口；各任务状态独立存放，队列内不传这些指针
 * 输入：无
 * 输出：A_System_Context* 生命周期覆盖整个任务运行期
 */
A_System_Context *A_System_GetContext(void);
/*
 * 说明：在短临界区创建全部静态队列，允许不同任务重复调用但不重建已有队列
 * 输入：p_context 板级上下文
 * 输出：uint32_t 非0表示七个队列全部创建成功
 */
uint32_t A_System_Initialize(A_System_Context *p_context);
/*
 * 说明：CAN任务从队列更新本地缓存、接收结果并发送命令及有效状态
 * 输入：p_context 板级上下文，now 当前1ms节拍
 * 输出：无
 */
void A_System_ProcessHostCan(A_System_Context *p_context, TickType_t now);
/*
 * 说明：推进六路采集并把完整快照送入覆盖队列，只由MfcTask调用
 * 输入：p_context 板级上下文，now 当前1ms节拍
 * 输出：无
 */
void A_System_ProcessMfc(A_System_Context *p_context, TickType_t now);
/*
 * 说明：绑定交接队列并推进ControlTask，不访问其他任务业务上下文
 * 输入：p_context 板级上下文，now 当前1ms节拍
 * 输出：无
 */
void A_System_ProcessControl(A_System_Context *p_context, TickType_t now);
#endif
