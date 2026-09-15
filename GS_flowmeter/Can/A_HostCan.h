/*
 * Created on: 2026年9月14日
 * Author: YXZ
 */
#ifndef CAN_A_HOST_CAN_H_
#define CAN_A_HOST_CAN_H_
#include "F_HostCan.h"

#define A_HOSTCAN_CHANNEL_COUNT (6U) // 六个MFC通道
#define A_HOSTCAN_VERSION_MAJOR (1U) // 固件主版本
#define A_HOSTCAN_VERSION_MINOR (6U) // 固件次版本
#define A_HOSTCAN_VERSION_PATCH (1U) // 固件修订版本
#define A_HOSTCAN_VERSION_DATE (260915U) // 固件版本日期YYMMDD
#define A_HOSTCAN_TX_CAPACITY (32U) // 软件回复队列容量
#define A_HOSTCAN_MAX_READ_COUNT (16U) // 单次连续读取上限
#define A_HOSTCAN_COMMAND_TIMEOUT_MS (3000U) // 包含在途轮询、RFSM、WSFD及RSFD的总期限
#define A_HOSTCAN_TX_TIMEOUT_MS (100U) // CAN发送等待上限
#define A_HOSTCAN_DATA_MAX_AGE_MS (2000U) // 实际流量最大允许数据年龄
#define A_HOSTCAN_EXECUTOR_MFC (1UL << 0U) // MFC执行入口已接入
#define A_HOSTCAN_EXECUTOR_VALVE (1UL << 1U) // 阀门执行入口已接入
#define A_HOSTCAN_VALID_ACTUAL (1UL << 0U) // 实际流量有效
#define A_HOSTCAN_VALID_CONFIRMED (1UL << 1U) // 仪器读回设定有效
#define A_HOSTCAN_VALID_SCALE (1UL << 2U) // 量程有效
#define A_HOSTCAN_VALID_UNIT (1UL << 3U) // 单位有效
#define A_HOSTCAN_VALID_DECIMAL (1UL << 4U) // 小数位有效
#define A_HOSTCAN_VALID_SOURCE (1UL << 5U) // 数字或模拟来源有效
#define A_HOSTCAN_VALID_VALVE (1UL << 6U) // MFC内部阀状态有效
#define A_HOSTCAN_VALID_ALARM (1UL << 7U) // MFC报警有效
#define A_HOSTCAN_VALID_TARGET (1UL << 8U) // MCU接受的目标流量有效
#define A_HOSTCAN_VALID_ADDRESS (1UL << 9U) // MFC通信地址已配置
#define A_HOSTCAN_VALVE_MASK (0x01FFUL) // 外部V1～V9掩码
#define A_HOSTCAN_VALVE_GROUP79 (0x0140UL) // V7及V9位
#define A_HOSTCAN_VALVE8 (0x0080UL) // V8位

typedef enum
{
    A_HOSTCAN_CODE_OK = 0x00,           // 执行成功
    A_HOSTCAN_CODE_ADDRESS = 0x04,      // 未定义的参数地址
    A_HOSTCAN_CODE_READ_ONLY = 0x05,    // 禁止写只读参数
    A_HOSTCAN_CODE_BUSY = 0x06,         // 当前有未完成写事务
    A_HOSTCAN_CODE_OFFLINE = 0x07,      // MFC离线
    A_HOSTCAN_CODE_NOT_READY = 0x08,    // 执行模块或设备未就绪
    A_HOSTCAN_CODE_INTERLOCK = 0x09,    // 阀门目标不满足联锁
    A_HOSTCAN_CODE_DEVICE_NG = 0x0A,    // 仪器拒绝
    A_HOSTCAN_CODE_DOWNSTREAM_TIMEOUT = 0x0B, // 下行设备响应超时
    A_HOSTCAN_CODE_STALE = 0x0C,        // 采集值过期
    A_HOSTCAN_CODE_VALUE = 0x0D,        // 非有限值、数量或编码无效
    A_HOSTCAN_CODE_RANGE = 0x0E,        // 数值超出范围
    A_HOSTCAN_CODE_EXECUTION_TIMEOUT = 0x0F, // 执行结果未在期限内提交
    A_HOSTCAN_CODE_DOWNSTREAM_PROTOCOL = 0x10, // 下行应答校验、地址或数据格式错误
    A_HOSTCAN_CODE_DOWNSTREAM_DRIVER = 0x11, // 下行驱动或恢复失败
    A_HOSTCAN_CODE_VERIFY = 0x12 // 写入后RSFD读回不一致
} A_HostCan_Code;

typedef enum
{
    A_HOSTCAN_COMMAND_SET_FLOW = 1, // 设置单路目标流量
    A_HOSTCAN_COMMAND_SET_VALVE,    // 单阀请求，已形成合法组目标
    A_HOSTCAN_COMMAND_SET_VALVES    // 完整九阀目标
} A_HostCan_Operation;

typedef enum
{
    A_HOSTCAN_COMMAND_IDLE = 0, // 没有待执行的写请求
    A_HOSTCAN_COMMAND_WAIT_QUEUE, // CAN任务已接受请求，下一步送入Control命令队列
    A_HOSTCAN_COMMAND_WAIT_RESULT, // 请求已交出，等待实际执行结果
    A_HOSTCAN_COMMAND_COMPLETED // 已有结果，等待生成CAN回复
} A_HostCan_Command_State;

typedef struct
{
    float actual_flow;          // 实际流量工程值
    float confirmed_flow;       // 仪器读回设定工程值
    float full_scale;           // 量程工程值
    float target_flow;          // MCU接受的目标工程值
    uint32_t valid_flags;       // 参数有效标志
    uint32_t online;            // 设备在线标志
    uint32_t initialize_state;  // 0未初始化，1初始化中，2就绪，3失败
    uint32_t unit;              // EX201单位码，0为cc，1为L
    uint32_t decimal_places;    // EX201小数位
    uint32_t sampled_ms;        // 实际流量最近有效采样毫秒时间
    uint32_t flow_source;       // 0数字，1模拟
    uint32_t internal_valve;    // 仪器内部阀状态
    uint32_t alarm;             // 仪器报警位
    uint32_t device_address;    // 下行设备地址
} A_HostCan_Channel;

typedef struct
{
    uint32_t state;           // 整机状态，启动默认INIT=0
    uint32_t faults;          // 整机故障位
    uint32_t link;            // 0未确认，1RS485，2CAN，3故障
    uint32_t valve_outputs;   // 已施加的九阀输出，不代表机械位置
    uint32_t valve_target;    // ControlTask管理的九阀目标
    uint32_t valve_state;     // 0空闲，1切换，2失败
    uint32_t valves_valid;    // 阀门输出及目标是否经过初始化
} A_HostCan_System;

typedef struct
{
    uint32_t sequence;            // MCU内部请求号，不增加CAN线上字段
    uint32_t started_ms;          // 接受命令时间，用于执行期限检查
    uint32_t index;               // 流量通道或阀门索引，从0开始
    uint32_t value;               // 原始写入32位数据
    uint32_t valve_target;        // 归一化后的九阀完整目标
    A_HostCan_Operation operation; // 交给ControlTask的业务操作
} A_HostCan_Command;

typedef struct
{
    F_HostCan_Context *p_transport;                     // 同属CAN任务的独立传输状态，初始化前绑定
    A_HostCan_Channel channels[A_HOSTCAN_CHANNEL_COUNT]; // CAN任务从队列更新的六路本地快照
    A_HostCan_System system;                            // 整机及外部阀快照
    A_HostCan_Command command;                         // CAN任务私有请求状态，不供其他任务直接领取
    F_CanUser_Message requester;                        // 延迟回复的原请求来源
    F_CanUser_Frame transmit[A_HOSTCAN_TX_CAPACITY];    // HostCan任务独占发送队列
    uint32_t transmit_read;                             // 队列读索引
    uint32_t transmit_count;                            // 队列占用数
    uint32_t transmit_active;                           // 队首已交给硬件
    uint32_t transmit_started_ms;                       // 硬件发送起始时间
    uint32_t recovering;                                // 恢复退避状态
    uint32_t recovery_ms;                               // 上次恢复尝试时间
    A_HostCan_Command_State command_state;              // 用枚举名称区分入队、执行和回复阶段
    uint32_t command_result;                            // 用户业务结果码，限定0～255
    uint32_t next_sequence;                             // 内部请求号
    uint32_t executors;                                 // 执行入口接入掩码，默认0
    uint32_t last_error_address;                        // 最近协议或业务错误地址
    uint32_t last_error_code;                           // 最近错误码
    uint32_t last_error_function;                       // 最近错误原功能码
    uint32_t invalid_frames;                            // 解码或校验失败计数
    uint32_t ignored_frames;                            // 非本机或未使用功能帧计数
    uint32_t failed_transmissions;                      // 发送失败或超时计数
    uint32_t initialized;                               // 应用初始化标志
    uint8_t self_address;                               // Type_FLOW节点地址，默认由任务传入1
} A_HostCan_Context;

/*
 * 说明：初始化上位机CAN业务，执行后端默认未就绪
 * 输入：p_context 长期有效且p_transport已绑定的上下文，self_address 主板节点地址0～126
 * 输出：uint32_t 非0成功，0失败
 */
uint32_t A_HostCan_Initialize(A_HostCan_Context *p_context, uint8_t self_address);
/*
 * 说明：有界处理接收、回复发送和执行结果；仅由HostCanStack调用
 * 输入：p_context 上下文，now_ms 单调递增的32位毫秒时间
 * 输出：无
 */
void A_HostCan_Process(A_HostCan_Context *p_context, uint32_t now_ms);
/*
 * 说明：CAN任务出队后更新一路本地快照，禁止其他任务或ISR调用
 * 输入：p_context 上下文，index 通道索引0～5，p_channel 完整快照
 * 输出：uint32_t 非0成功
 */
uint32_t A_HostCan_PublishChannel(A_HostCan_Context *p_context, uint32_t index, const A_HostCan_Channel *p_channel);
/*
 * 说明：仅CAN任务更新本地整机快照；未来Control状态必须先经队列传入
 * 输入：p_context 上下文，p_system 完整快照
 * 输出：uint32_t 非0成功
 */
uint32_t A_HostCan_PublishSystem(A_HostCan_Context *p_context, const A_HostCan_System *p_system);
/*
 * 说明：仅CAN任务从遥测消息取得链路后更新本地字段，不覆盖整机及阀门字段
 * 输入：p_context 上下文，link 0待确认、1RS485、2CAN、3故障
 * 输出：uint32_t 非0成功
 */
uint32_t A_HostCan_PublishMfcLink(A_HostCan_Context *p_context, uint32_t link);
/*
 * 说明：仅CAN任务根据队列收到的就绪状态更新执行能力，禁止跨任务调用
 * 输入：p_context 上下文，executors MFC/VALVE掩码；撤销对应类型时使旧命令失效
 * 输出：无
 */
void A_HostCan_SetExecutors(A_HostCan_Context *p_context, uint32_t executors);
/*
 * 说明：仅CAN任务提取私有待发请求，再按值送入host_command_queue
 * 输入：p_context 上下文，p_command 输出命令
 * 输出：uint32_t 非0表示取到命令；不得从ISR调用
 */
uint32_t A_HostCan_TakeCommand(A_HostCan_Context *p_context, A_HostCan_Command *p_command);
/*
 * 说明：仅CAN任务检查本地请求有效性，再向两个状态队列发布结果
 * 输入：p_context 上下文，sequence 内部请求号，now_ms 当前毫秒时间
 * 输出：uint32_t 非0有效
 */
uint32_t A_HostCan_CommandActive(A_HostCan_Context *p_context, uint32_t sequence, uint32_t now_ms);
/*
 * 说明：仅CAN任务接收host_result_queue后提交实际结果，不允许跨任务调用
 * 输入：p_context 上下文，sequence 内部请求号，code 0～255业务结果，now_ms 当前毫秒时间
 * 输出：uint32_t 非0接收；已超时或旧请求的结果返回0
 */
uint32_t A_HostCan_CompleteCommand(A_HostCan_Context *p_context, uint32_t sequence, uint8_t code, uint32_t now_ms);
#endif
