/*
 * A_EX201.h
 *
 * Created on: 2026年9月14日
 * Author: YXZ
 */

#ifndef MFC_A_EX201_H_
#define MFC_A_EX201_H_

#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"

#include "F_EX201.h"

#define A_EX201_DEVICE_INFO_FULL_SCALE_VALID    (1UL << 0U) // 满刻度流量尾数有效
#define A_EX201_DEVICE_INFO_DECIMAL_VALID       (1UL << 1U) // 流量小数位有效
#define A_EX201_DEVICE_INFO_UNIT_VALID          (1UL << 2U) // 流量单位有效
#define A_EX201_DEVICE_INFO_SOURCE_VALID        (1UL << 3U) // 流量设定来源有效
#define A_EX201_DEVICE_INFO_VALVE_SETTING_VALID (1UL << 4U) // 数字阀门设定有效
#define A_EX201_DEVICE_INFO_VALVE_STATE_VALID   (1UL << 5U) // 当前阀门状态有效
#define A_EX201_DEVICE_INFO_ALARM_VALID         (1UL << 6U) // 报警状态有效

#define A_EX201_ALARM_SENSOR_ERROR (1U << 0U) // 传感器异常
#define A_EX201_ALARM_VALVE_HEAT   (1U << 1U) // 阀门过热
#define A_EX201_ALARM_MEMORY_ERROR (1U << 2U) // 设定值存储回路异常

typedef enum
{
    A_EX201_STATE_IDLE = 0,          // 空闲，可以启动新事务
    A_EX201_STATE_WAIT_TX_COMPLETE,  // 等待UART完成整帧发送
    A_EX201_STATE_WAIT_RESPONSE,     // 等待EX-201S响应帧
    A_EX201_STATE_COMPLETE,          // 事务完成，等待取走结果
    A_EX201_STATE_ERROR              // 事务失败，等待取走错误
} A_EX201_State;

typedef enum
{
    A_EX201_RESULT_OK = 0,             // 事务成功
    A_EX201_RESULT_INVALID_ARGUMENT,   // 输入参数无效
    A_EX201_RESULT_NOT_INITIALIZED,    // 事务模块尚未初始化
    A_EX201_RESULT_INITIALIZE_ERROR,   // 功能层或硬件层初始化失败
    A_EX201_RESULT_BUSY,               // 事务或结果尚未处理完毕
    A_EX201_RESULT_NO_RESULT,          // 当前没有可读取的事务结果
    A_EX201_RESULT_SEND_ERROR,         // 请求帧发送启动失败
    A_EX201_RESULT_RECEIVE_ERROR,      // 响应接收或UART发生错误
    A_EX201_RESULT_TX_TIMEOUT,         // UART发送完成超时
    A_EX201_RESULT_RESPONSE_TIMEOUT,   // EX-201S响应超时
    A_EX201_RESULT_PROTOCOL_ERROR,     // 响应帧格式或校验和错误
    A_EX201_RESULT_RESPONSE_MISMATCH,  // 响应地址或命令与请求不匹配
    A_EX201_RESULT_DEVICE_NG,          // EX-201S返回NG
    A_EX201_RESULT_RECOVERY_ERROR      // 通信异常恢复失败
} A_EX201_Result;

typedef enum
{
    A_EX201_FLOW_UNIT_CC = 0, // 流量单位为cc
    A_EX201_FLOW_UNIT_LITER   // 流量单位为L
} A_EX201_FlowUnit;

typedef enum
{
    A_EX201_FLOW_SOURCE_DIGITAL = 0, // 数字通信设定流量
    A_EX201_FLOW_SOURCE_ANALOG       // 模拟输入设定流量
} A_EX201_FlowSource;

typedef enum
{
    A_EX201_VALVE_SETTING_FULL_OPEN = 0, // 数字设定为全打开
    A_EX201_VALVE_SETTING_CONTROL,       // 数字设定为调节控制
    A_EX201_VALVE_SETTING_FULL_CLOSE     // 数字设定为全关闭
} A_EX201_ValveSetting;

typedef enum
{
    A_EX201_VALVE_STATE_FULL_OPEN = 0, // 当前阀门全打开
    A_EX201_VALVE_STATE_CONTROL,       // 当前阀门处于调节控制
    A_EX201_VALVE_STATE_FULL_CLOSE,    // 当前阀门全关闭
    A_EX201_VALVE_STATE_HALF_OPEN      // 当前阀门开度为50%
} A_EX201_ValveState;

typedef enum
{
    A_EX201_OPERATION_NONE = 0,        // 当前没有业务操作
    A_EX201_OPERATION_GENERIC,         // 通用原始请求
    A_EX201_OPERATION_SET_FLOW,        // 设置数字流量尾数
    A_EX201_OPERATION_READ_SET_FLOW,   // 读取数字设定流量尾数
    A_EX201_OPERATION_READ_ACTUAL_FLOW, // 读取瞬时流量尾数
    A_EX201_OPERATION_CLOSE_FLOW,      // 数字阀门全关闭
    A_EX201_OPERATION_READ_FULL_SCALE, // 读取满刻度流量尾数
    A_EX201_OPERATION_READ_DECIMAL,    // 读取流量小数位
    A_EX201_OPERATION_READ_UNIT,       // 读取流量单位
    A_EX201_OPERATION_READ_SOURCE,     // 读取流量设定来源
    A_EX201_OPERATION_READ_VALVE_SETTING, // 读取数字阀门设定
    A_EX201_OPERATION_READ_VALVE_STATE,   // 读取当前阀门状态
    A_EX201_OPERATION_READ_ALARM          // 读取报警状态
} A_EX201_Operation;

typedef struct
{
    uint32_t valid_flags;                    // 已成功读取的参数有效标志
    uint16_t full_scale_mantissa;            // 满刻度流量尾数，范围0001～9999
    uint8_t decimal_places;                  // 流量小数位，范围0～3
    A_EX201_FlowUnit flow_unit;              // 流量单位
    A_EX201_FlowSource flow_source;          // 流量设定来源
    A_EX201_ValveSetting valve_setting;      // 数字阀门设定
    A_EX201_ValveState valve_state;          // 当前实际阀门状态
    uint8_t alarm_state;                     // 报警位组合，范围0～7
} A_EX201_DeviceInfo;

typedef struct
{
    F_EX201_Context function_context;                              // EX-201S功能层上下文
    F_EX201_Response response;                                     // 已解析的当前响应
    A_EX201_State state;                                           // 当前事务状态
    A_EX201_Result result;                                         // 最近一次事务结果
    A_EX201_Operation operation;                                   // 当前业务操作类型
    TickType_t state_start_tick;                                   // 当前等待状态起始节拍
    uint8_t request_frame[EX201_MAX_REQUEST_FRAME_LENGTH];          // 当前请求帧缓冲区
    uint8_t response_frame[EX201_MAX_RESPONSE_FRAME_LENGTH];        // 当前响应帧缓冲区
    char expected_command[EX201_COMMAND_LENGTH];                    // 当前请求的四字节命令
    uint16_t expected_address;                                     // 当前请求的目标地址
    uint32_t request_sequence;                                     // 已成功启动的请求序号
    uint32_t initialized;                                          // 事务模块初始化状态
} A_EX201_Context;

/*
 * 说明：初始化EX-201S事务模块及其下层模块
 * 输入：p_context EX-201S事务上下文
 * 输出：A_EX201_Result 初始化结果
 */
A_EX201_Result A_EX201_Initialize(A_EX201_Context *p_context);

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
    A_EX201_Context *p_context,
    uint16_t address,
    const char command[EX201_COMMAND_LENGTH],
    const uint8_t *p_data,
    size_t data_length,
    TickType_t current_tick);

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
    TickType_t current_tick);

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
    TickType_t current_tick);

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
    TickType_t current_tick);

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
    TickType_t current_tick);

/*
 * 说明：使用RMFS指令读取满刻度流量尾数
 * 输入：p_context    EX-201S事务上下文
 *      address      流量计通信地址
 *      current_tick 当前FreeRTOS系统节拍
 * 输出：A_EX201_Result 请求启动结果
 */
A_EX201_Result A_EX201_ReadFullScale(
    A_EX201_Context *p_context,
    uint16_t address,
    TickType_t current_tick);

/*
 * 说明：使用RDPP指令读取流量小数位
 * 输入：p_context    EX-201S事务上下文
 *      address      流量计通信地址
 *      current_tick 当前FreeRTOS系统节拍
 * 输出：A_EX201_Result 请求启动结果
 */
A_EX201_Result A_EX201_ReadDecimalPlaces(
    A_EX201_Context *p_context,
    uint16_t address,
    TickType_t current_tick);

/*
 * 说明：使用RFRU指令读取流量单位
 * 输入：p_context    EX-201S事务上下文
 *      address      流量计通信地址
 *      current_tick 当前FreeRTOS系统节拍
 * 输出：A_EX201_Result 请求启动结果
 */
A_EX201_Result A_EX201_ReadFlowUnit(
    A_EX201_Context *p_context,
    uint16_t address,
    TickType_t current_tick);

/*
 * 说明：使用RFSM指令读取流量设定来源
 * 输入：p_context    EX-201S事务上下文
 *      address      流量计通信地址
 *      current_tick 当前FreeRTOS系统节拍
 * 输出：A_EX201_Result 请求启动结果
 */
A_EX201_Result A_EX201_ReadFlowSource(
    A_EX201_Context *p_context,
    uint16_t address,
    TickType_t current_tick);

/*
 * 说明：使用RVSS指令读取数字阀门设定
 * 输入：p_context    EX-201S事务上下文
 *      address      流量计通信地址
 *      current_tick 当前FreeRTOS系统节拍
 * 输出：A_EX201_Result 请求启动结果
 */
A_EX201_Result A_EX201_ReadValveSetting(
    A_EX201_Context *p_context,
    uint16_t address,
    TickType_t current_tick);

/*
 * 说明：使用RCVS指令读取当前阀门状态
 * 输入：p_context    EX-201S事务上下文
 *      address      流量计通信地址
 *      current_tick 当前FreeRTOS系统节拍
 * 输出：A_EX201_Result 请求启动结果
 */
A_EX201_Result A_EX201_ReadValveState(
    A_EX201_Context *p_context,
    uint16_t address,
    TickType_t current_tick);

/*
 * 说明：使用RALM指令读取报警状态
 * 输入：p_context    EX-201S事务上下文
 *      address      流量计通信地址
 *      current_tick 当前FreeRTOS系统节拍
 * 输出：A_EX201_Result 请求启动结果
 */
A_EX201_Result A_EX201_ReadAlarmState(
    A_EX201_Context *p_context,
    uint16_t address,
    TickType_t current_tick);

/*
 * 说明：推进EX-201S非阻塞事务状态机，必须由MfcTask周期调用
 * 输入：p_context    EX-201S事务上下文
 *      current_tick 当前FreeRTOS系统节拍
 * 输出：无
 */
void A_EX201_Process(
    A_EX201_Context *p_context,
    TickType_t current_tick);

/*
 * 说明：查询事务或尚未取走的结果是否占用客户端
 * 输入：p_context EX-201S事务上下文
 * 输出：uint32_t 非0表示不能启动新事务，0表示空闲
 */
uint32_t A_EX201_IsBusy(A_EX201_Context *p_context);

/*
 * 说明：读取已完成事务的结果并使客户端恢复空闲
 * 输入：p_context  EX-201S事务上下文
 *      p_response 成功或设备返回NG时的响应数据；普通错误时允许为NULL
 * 输出：A_EX201_Result 事务执行结果、忙状态或无结果状态
 */
A_EX201_Result A_EX201_GetResult(
    A_EX201_Context *p_context,
    F_EX201_Response *p_response);

/*
 * 说明：读取RSFD或RCFR业务操作返回的流量尾数并恢复空闲
 * 输入：p_context       EX-201S事务上下文
 *      p_flow_mantissa 输出的有符号流量尾数
 * 输出：A_EX201_Result 事务及流量数据解析结果
 */
A_EX201_Result A_EX201_GetFlowResult(
    A_EX201_Context *p_context,
    int32_t *p_flow_mantissa);

/*
 * 说明：读取WSFD或WVSS业务操作的执行结果并恢复空闲
 * 输入：p_context EX-201S事务上下文
 * 输出：A_EX201_Result 事务执行结果
 */
A_EX201_Result A_EX201_GetCommandResult(A_EX201_Context *p_context);

/*
 * 说明：解析一个设备初始化或状态读取结果并更新对应有效字段
 * 输入：p_context     EX-201S事务上下文
 *      p_device_info 设备信息结构体
 * 输出：A_EX201_Result 事务及参数解析结果
 */
A_EX201_Result A_EX201_GetDeviceInfoResult(
    A_EX201_Context *p_context,
    A_EX201_DeviceInfo *p_device_info);

#endif /* MFC_A_EX201_H_ */
