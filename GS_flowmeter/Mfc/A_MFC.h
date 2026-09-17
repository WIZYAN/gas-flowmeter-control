/*
 * Created on: 2026年9月15日
 * Author: CI
 */
#ifndef MFC_A_MFC_H_
#define MFC_A_MFC_H_
#include "A_EX201.h"

#define A_MFC_CHANNEL_COUNT (6U) // 固定轮询六路，包括第六路预留口
#define A_MFC_ACTUAL_PERIOD_MS (500U) // 每路瞬时流量目标查询间隔
#define A_MFC_STATUS_SLOT_MS (1000U) // 每路每次查询一个慢速参数的目标间隔
#define A_MFC_RETRY_MS (5000U) // 离线设备及硬件故障恢复间隔
#define A_MFC_ERROR_GUARD_MS (50U) // 错误后总线静默时间，实机需核对迟到帧
#define A_MFC_FAILURE_LIMIT (3U) // 已就绪通道连续失败次数上限
#define A_MFC_VALID_ACTUAL (1UL << 0U) // 瞬时流量尾数有效
#define A_MFC_VALID_CONFIRMED (1UL << 1U) // 仪器数字设定尾数有效
#define A_MFC_VALID_TARGET (1UL << 2U) // MCU最近一次写入且读回确认的目标有效

typedef enum
{
    A_MFC_COMMAND_SET_FLOW = 1 // 单次设置数字流量，不切换内部或外部阀门
} A_MFC_CommandOperation;

typedef enum
{
    A_MFC_COMMAND_OK = 0,       // 写入及读回一致
    A_MFC_COMMAND_OFFLINE,      // 设备离线
    A_MFC_COMMAND_NOT_READY,    // 参数或数字控制来源未就绪
    A_MFC_COMMAND_VALUE,        // 非法值或超出仪器分辨率
    A_MFC_COMMAND_RANGE,        // 目标超出量程
    A_MFC_COMMAND_DEVICE_NG,    // 仪器拒绝
    A_MFC_COMMAND_TIMEOUT,      // 下行事务超时，写入可能已发生
    A_MFC_COMMAND_EXPIRED,      // 请求过期或被上位机通信恢复撤销
    A_MFC_COMMAND_PROTOCOL,     // 应答协议错误
    A_MFC_COMMAND_DRIVER,       // 串口或恢复失败
    A_MFC_COMMAND_VERIFY        // RSFD读回与写入尾数不一致
} A_MFC_CommandCode;

typedef enum
{
    A_MFC_WRITE_IDLE = 0, // 没有写请求
    A_MFC_WRITE_PENDING,  // 等待当前轮询结束
    A_MFC_WRITE_SOURCE,   // 等待RFSM确认数字来源
    A_MFC_WRITE_START,    // 准备发送一次WSFD
    A_MFC_WRITE_ACK,      // 等待WSFD的OK或NG
    A_MFC_WRITE_READBACK, // 准备发送RSFD
    A_MFC_WRITE_VERIFY,   // 等待RSFD读回
    A_MFC_WRITE_DONE      // 结果待入队，期间可继续轮询
} A_MFC_WriteState;

typedef struct
{
    uint32_t sequence; // 上位机请求内部编号，不增加CAN线上字段
    uint32_t index; // 通道索引0～5
    A_MFC_CommandOperation operation; // 业务操作
    float target_flow; // 本通道单位的工程值
    TickType_t started_tick; // CAN接受请求的时间
    TickType_t timeout_ticks; // 总期限，包含排队及通信
} A_MFC_Command;

typedef struct
{
    uint32_t sequence; // 关联原请求，迟到结果不能完成新请求
    A_MFC_CommandCode code; // 业务执行结果，由Control层映射到CAN_USER
    A_EX201_Result transport_result; // 具体EX201结果，便于调试
} A_MFC_CommandResult;

typedef enum
{
    A_MFC_INIT_PENDING = 0, // 尚未开始
    A_MFC_INIT_RUNNING,     // 正在逐参数初始化
    A_MFC_INIT_READY,       // 九个初始参数全部读回
    A_MFC_INIT_FAILED       // 离线，等待退避后重新初始化
} A_MFC_InitState;

typedef enum
{
    A_MFC_READ_SCALE = 0, // RMFS
    A_MFC_READ_DECIMAL,   // RDPP
    A_MFC_READ_UNIT,      // RFRU
    A_MFC_READ_ACTUAL,    // RCFR
    A_MFC_READ_CONFIRMED, // RSFD
    A_MFC_READ_SOURCE,    // RFSM
    A_MFC_READ_SETTING,   // RVSS
    A_MFC_READ_VALVE,     // RCVS
    A_MFC_READ_ALARM,     // RALM
    A_MFC_READ_COUNT      // 初始化操作数量
} A_MFC_ReadOperation;

typedef struct
{
    uint16_t addresses[A_MFC_CHANNEL_COUNT]; // 六台设备的独立地址，必须互不重复
} A_MFC_Config;

typedef struct
{
    A_EX201_DeviceInfo device_info; // 本通道设备参数，绝不借用其他通道参数
    int32_t actual_mantissa;        // 本通道瞬时流量尾数，允许负值
    int32_t confirmed_mantissa;     // 仪器读回的数字设定尾数
    uint16_t target_mantissa;       // MCU最后写入并读回确认的尾数
    uint32_t flow_valid;            // 流量字段有效标志
    uint32_t online;                // 已收到有效应答且未判离线
    uint32_t revision;              // 快照更新序号
    uint32_t consecutive_failures;  // 连续失败次数
    uint32_t last_error;            // 最近一次事务结果，A_EX201_Result
    A_MFC_InitState initialize_state; // 本通道初始化状态
    uint32_t initialize_step;       // 下一个初始化参数索引
    uint32_t status_step;           // 下一个慢速参数索引
    uint32_t prefer_status;         // 流量与状态同时到期时交替，避免饿死状态查询
    TickType_t sampled_tick;        // 瞬时流量最近一次有效采样时间
    TickType_t actual_attempt_tick; // 最近一次瞬时流量查询发起时间
    TickType_t status_attempt_tick; // 最近一次慢速参数查询发起时间
    TickType_t retry_tick;          // 进入离线退避的时间
    uint16_t address;               // 本通道EX201通信地址
} A_MFC_Channel;

typedef struct
{
    A_EX201_Context *p_transaction;              // 六路共用的独立EX201事务，初始化前绑定
    A_MFC_Channel *p_channels;                   // 六个独立通道的固定数组，初始化前绑定且生命周期覆盖任务
    uint32_t next_channel;                        // 下一轮优先检查的通道
    uint32_t active_channel;                      // 当前事务通道索引
    A_MFC_ReadOperation active_operation;         // 当前只读操作
    uint32_t active;                              // 有事务等待取结果
    uint32_t configured;                          // 六路地址已经校验和初始化
    uint32_t transport_ready;                     // 串口客户端可用
    uint32_t transport_attempted;                 // 已尝试初始化或恢复
    uint32_t guard_active;                        // 错误后静默等待
    uint32_t valid_responses;                     // 链路确认计数，最多累计到2
    TickType_t transport_retry_tick;              // 硬件重试时间
    TickType_t guard_tick;                        // 错误后静默起点
    A_MFC_Command write_command;                  // MfcTask从队列复制的当前命令
    A_MFC_CommandResult write_result;             // 结果队列满时继续保留
    A_MFC_WriteState write_state;                 // 写入及读回状态
    uint16_t write_mantissa;                      // 经本通道量程和分辨率检查的尾数
    uint32_t write_attempted;                     // 已尝试发送WSFD，失败时结果可能不确定
} A_MFC_Context;

/*
 * 说明：生成六路默认地址配置，实际部署前按仪器地址修改
 * 输入：p_config 输出配置
 * 输出：无
 */
void A_MFC_DefaultConfig(A_MFC_Config *p_config);
/*
 * 说明：配置六路轮询；只在启动时调用，硬件初始化由Process自动重试
 * 输入：p_context 长期有效且p_channels/p_transaction已绑定的上下文，p_config 六路地址，now 当前节拍
 * 输出：uint32_t 非0成功，0表示地址无效、重复或已配置
 */
uint32_t A_MFC_Initialize(A_MFC_Context *p_context, const A_MFC_Config *p_config, TickType_t now);
/*
 * 说明：非阻塞推进一个共享事务及六路公平轮询，仅由MfcTask调用
 * 输入：p_context 上下文，now 当前节拍
 * 输出：无
 */
void A_MFC_Process(A_MFC_Context *p_context, TickType_t now);
/*
 * 说明：读取本任务持有的通道状态，不得从其他任务直接读此指针
 * 输入：p_context 上下文，index 通道0～5
 * 输出：const A_MFC_Channel* 有效通道指针，无效参数返回NULL
 */
const A_MFC_Channel *A_MFC_GetChannel(const A_MFC_Context *p_context, uint32_t index);
/*
 * 说明：取得当前EX201链路状态；不代表CAN/RS485双后端探测已实现
 * 输入：p_context 上下文
 * 输出：uint32_t 0待确认，1有效RS485，3离线或硬件故障
 */
uint32_t A_MFC_GetLink(const A_MFC_Context *p_context);
/*
 * 说明：MfcTask领取队列命令后复制到自身上下文，不得跨任务调用
 * 输入：p_context 上下文，p_command 命令副本
 * 输出：uint32_t 非0接收，0表示忙或输入指针无效
 */
uint32_t A_MFC_SubmitCommand(A_MFC_Context *p_context, const A_MFC_Command *p_command);
/*
 * 说明：推进一次写入状态机，必须在同轮A_MFC_Process之前调用
 * 输入：p_context 上下文，now 当前节拍，authorized 根据本任务出队的CAN状态副本判断
 * 输出：无
 */
void A_MFC_ProcessCommand(A_MFC_Context *p_context, TickType_t now, uint32_t authorized);
/*
 * 说明：查看待提交结果，队列满时结果不丢弃
 * 输入：p_context 上下文，p_result 输出结果
 * 输出：uint32_t 非0表示结果可取
 */
uint32_t A_MFC_GetCommandResult(const A_MFC_Context *p_context, A_MFC_CommandResult *p_result);
/*
 * 说明：结果成功入队后释放写请求槽，仅由MfcTask调用
 * 输入：p_context 上下文
 * 输出：无
 */
void A_MFC_ReleaseCommand(A_MFC_Context *p_context);
#endif
