/*
 * F_EX201.h
 *
 * Created on: 2026年9月12日
 * Author: YXZ
 */

#ifndef MFC_F_EX201_H_
#define MFC_F_EX201_H_

#include <stddef.h>
#include <stdint.h>

#include "H_EX201.h"

#define EX201_REQUEST_START_CHARACTER  ((uint8_t) '@')  // 请求帧起始字符
#define EX201_RESPONSE_START_CHARACTER ((uint8_t) '%')  // 响应帧起始字符
#define EX201_FRAME_END_CHARACTER      ((uint8_t) '\r') // 协议帧结束字符

#define EX201_ID_LENGTH                (3U)  // 通信ID长度，3字节
#define EX201_COMMAND_LENGTH           (4U)  // 指令长度，4字节
#define EX201_RESPONSE_CODE_LENGTH     (2U)  // 响应结果长度，2字节
#define EX201_CHECKSUM_LENGTH          (2U)  // 校验和长度，2字节
#define EX201_MAX_DATA_LENGTH          (32U) // 软件允许的最大数据长度
#define EX201_FLOW_MANTISSA_LENGTH     (4U)  // 流量尾数固定长度，4个十进制字符
#define EX201_SIGNED_FLOW_LENGTH       (5U)  // 瞬时流量长度，符号加4个十进制字符
#define EX201_FLOW_MANTISSA_MAX        (9999U) // 流量尾数允许的最大值

#define EX201_ADDRESS_MIN              (1U)   // 协议地址字段允许的最小值
#define EX201_ADDRESS_MAX              (999U) // 协议地址字段允许的最大值

#define EX201_REQUEST_FIXED_LENGTH      (1U + EX201_ID_LENGTH + EX201_COMMAND_LENGTH + EX201_CHECKSUM_LENGTH + 1U) // 不包含数据的请求帧长度
#define EX201_RESPONSE_FIXED_LENGTH     (1U + EX201_ID_LENGTH + EX201_COMMAND_LENGTH + EX201_RESPONSE_CODE_LENGTH + EX201_CHECKSUM_LENGTH + 1U) // 不包含数据的响应帧长度
#define EX201_MAX_REQUEST_FRAME_LENGTH  (EX201_REQUEST_FIXED_LENGTH + EX201_MAX_DATA_LENGTH)   // 最大请求帧长度
#define EX201_MAX_RESPONSE_FRAME_LENGTH (EX201_RESPONSE_FIXED_LENGTH + EX201_MAX_DATA_LENGTH) // 最大响应帧长度

typedef enum
{
    F_EX201_PROTOCOL_RESULT_OK = 0,             // 协议处理成功
    F_EX201_PROTOCOL_RESULT_INVALID_ARGUMENT,   // 输入参数无效
    F_EX201_PROTOCOL_RESULT_BUFFER_TOO_SMALL,   // 输出缓冲区容量不足
    F_EX201_PROTOCOL_RESULT_INVALID_FRAME,      // 协议帧格式无效
    F_EX201_PROTOCOL_RESULT_CHECKSUM_ERROR      // 协议帧校验和错误
} F_EX201_ProtocolResult;

typedef enum
{
    F_EX201_RESPONSE_STATUS_OK = 0, // 设备返回OK
    F_EX201_RESPONSE_STATUS_NG      // 设备返回NG
} F_EX201_ResponseStatus;

typedef enum
{
    F_EX201_TRANSPORT_RESULT_OK = 0,             // 传输操作成功
    F_EX201_TRANSPORT_RESULT_INVALID_ARGUMENT,   // 输入参数无效
    F_EX201_TRANSPORT_RESULT_NOT_INITIALIZED,    // 传输模块尚未初始化
    F_EX201_TRANSPORT_RESULT_BUSY,               // 上一次发送尚未完成
    F_EX201_TRANSPORT_RESULT_NO_FRAME,           // 暂时没有完整响应帧
    F_EX201_TRANSPORT_RESULT_BUFFER_TOO_SMALL,   // 输出帧缓冲区容量不足
    F_EX201_TRANSPORT_RESULT_RECEIVE_OVERFLOW,   // 接收缓冲区溢出
    F_EX201_TRANSPORT_RESULT_UART_ERROR,         // UART接收错误
    F_EX201_TRANSPORT_RESULT_DRIVER_ERROR        // 硬件驱动调用失败
} F_EX201_TransportResult;

typedef struct
{
    uint16_t address;                              // 流量计通信地址
    char command[EX201_COMMAND_LENGTH + 1U];       // 四字节命令及字符串结束符
    F_EX201_ResponseStatus status;                 // 设备OK或NG响应状态
    uint8_t data[EX201_MAX_DATA_LENGTH];           // 响应数据
    uint32_t data_length;                          // 实际响应数据长度
} F_EX201_Response;

typedef struct
{
    H_EX201_Context hardware_context;                         // RS485硬件层上下文
    uint8_t response_frame[EX201_MAX_RESPONSE_FRAME_LENGTH];  // 正在接收的EX-201S响应帧
    uint32_t response_length;                                 // 当前响应帧长度
    uint32_t initialized;                                     // 功能模块初始化状态
} F_EX201_Context;

/*
 * 说明：初始化EX-201S功能层和RS485硬件层
 * 输入：p_context 功能模块上下文
 * 输出：F_EX201_TransportResult 初始化结果
 */
F_EX201_TransportResult F_EX201_Initialize(F_EX201_Context *p_context);

/*
 * 说明：生成EX-201S请求协议帧
 * 输入：address        流量计通信地址
 *      command        四字节命令，不包含字符串结束符
 *      p_data         请求数据，data_length为0时允许为NULL
 *      data_length    请求数据长度
 *      p_frame        输出帧缓冲区
 *      frame_capacity 输出帧缓冲区容量
 *      p_frame_length 实际生成的协议帧长度
 * 输出：F_EX201_ProtocolResult 协议处理结果
 */
F_EX201_ProtocolResult F_EX201_EncodeRequest(
    uint16_t address,
    const char command[EX201_COMMAND_LENGTH],
    const uint8_t *p_data,
    size_t data_length,
    uint8_t *p_frame,
    size_t frame_capacity,
    size_t *p_frame_length);

/*
 * 说明：解析一帧完整的EX-201S响应协议帧
 * 输入：p_frame      完整响应帧
 *      frame_length 响应帧长度
 *      p_response   响应解析结果，解析失败时保持原值
 * 输出：F_EX201_ProtocolResult 协议处理结果
 */
F_EX201_ProtocolResult F_EX201_DecodeResponse(
    const uint8_t *p_frame,
    size_t frame_length,
    F_EX201_Response *p_response);

/*
 * 说明：将流量尾数转换为EX-201S使用的四位十进制ASCII数据
 * 输入：flow_mantissa 流量尾数，范围0000～9999
 *      p_data        输出的四字节ASCII数据
 * 输出：F_EX201_ProtocolResult 转换结果
 */
F_EX201_ProtocolResult F_EX201_EncodeFlowValue(
    uint16_t flow_mantissa,
    uint8_t p_data[EX201_FLOW_MANTISSA_LENGTH]);

/*
 * 说明：将EX-201S流量ASCII数据转换为有符号流量尾数
 * 输入：p_data         四位无符号数据或符号加四位瞬时流量数据
 *      data_length    数据长度
 *      p_flow_mantissa 输出的有符号流量尾数
 * 输出：F_EX201_ProtocolResult 转换结果
 */
F_EX201_ProtocolResult F_EX201_DecodeFlowValue(
    const uint8_t *p_data,
    size_t data_length,
    int32_t *p_flow_mantissa);

/*
 * 说明：将一至四位无符号十进制ASCII数据转换为整数
 * 输入：p_data      无符号十进制ASCII数据
 *      data_length 数据长度，范围1～4字节
 *      p_value     输出的整数值
 * 输出：F_EX201_ProtocolResult 转换结果
 */
F_EX201_ProtocolResult F_EX201_DecodeUnsignedValue(
    const uint8_t *p_data,
    size_t data_length,
    uint32_t *p_value);

/*
 * 说明：通过硬件层异步发送一帧EX-201S数据
 * 输入：p_context  功能模块上下文
 *      p_data      待发送帧
 *      data_length 待发送帧长度
 * 输出：F_EX201_TransportResult 发送启动结果
 */
F_EX201_TransportResult F_EX201_SendFrame(
    F_EX201_Context *p_context,
    const uint8_t *p_data,
    size_t data_length);

/*
 * 说明：从硬件层取字节并组装一帧完整EX-201S响应
 * 输入：p_context     功能模块上下文
 *      p_frame       输出帧缓冲区
 *      frame_capacity 输出帧缓冲区容量
 *      p_frame_length 实际响应帧长度
 * 输出：F_EX201_TransportResult 接收结果
 */
F_EX201_TransportResult F_EX201_ReceiveFrame(
    F_EX201_Context *p_context,
    uint8_t *p_frame,
    size_t frame_capacity,
    size_t *p_frame_length);

/*
 * 说明：通过硬件层恢复异常的RS485通信
 * 输入：p_context 功能模块上下文
 * 输出：F_EX201_TransportResult 恢复结果
 */
F_EX201_TransportResult F_EX201_Recover(F_EX201_Context *p_context);

/*
 * 说明：查询硬件层是否仍在发送EX-201S数据
 * 输入：p_context 功能模块上下文
 * 输出：uint32_t 非0表示正在发送，0表示发送空闲
 */
uint32_t F_EX201_IsTransmitBusy(F_EX201_Context *p_context);

/*
 * 说明：清空EX-201S功能层和硬件层接收状态
 * 输入：p_context 功能模块上下文
 * 输出：无
 */
void F_EX201_ResetReceiver(F_EX201_Context *p_context);

#endif /* MFC_F_EX201_H_ */
