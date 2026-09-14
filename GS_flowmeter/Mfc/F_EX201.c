/*
 * F_EX201.c
 *
 * Created on: 2026年9月12日
 * Author: YXZ
 */

#include "F_EX201.h"

#include <stdbool.h>

/*
 * 说明：判断四字节命令是否包含有效的可见ASCII字符
 * 输入：command 四字节命令
 * 输出：bool true表示命令有效，false表示命令无效
 */
static bool F_EX201_IsCommandValid(const char command[EX201_COMMAND_LENGTH])
{
    size_t index = 0U; // 命令字符索引

    for (index = 0U; index < EX201_COMMAND_LENGTH; index++)
    {
        if (((uint8_t) command[index] < 0x21U) ||
            ((uint8_t) command[index] > 0x7EU))//0x21~0x7E为ASCII码
        {
            return false;
        }
    }

    return true;
}

/*
 * 说明：判断EX-201S请求数据是否只包含协议允许的可见ASCII字符
 * 输入：data        请求数据，data_length为0时允许为NULL
 *      data_length 请求数据长度
 * 输出：bool true表示数据有效，false表示数据无效
 */
static bool F_EX201_IsDataValid(
    const uint8_t *data,
    size_t data_length)
{
    size_t data_index = 0U; // 请求数据索引

    if (0U == data_length)
    {
        return true;
    }

    if (data == NULL)
    {
        return false;
    }

    for (data_index = 0U; data_index < data_length; data_index++)
    {
        if ((data[data_index] < 0x20U) ||
            (data[data_index] > 0x7EU) ||
            (data[data_index] == EX201_REQUEST_START_CHARACTER) ||
            (data[data_index] == EX201_RESPONSE_START_CHARACTER))
        {
            return false;
        }
    }

    return true;
}

/*
 * 说明：计算EX-201S协议帧的累加校验和
 * 输入：data        参与校验的数据
 *      data_length 参与校验的数据长度
 * 输出：uint8_t 累加结果的低8位
 */
static uint8_t F_EX201_CalculateChecksum(
    const uint8_t *data,
    size_t data_length)
{
    uint8_t checksum = 0U; // 累加校验和
    size_t index = 0U;     // 数据索引

    for (index = 0U; index < data_length; index++)
    {
        checksum = (uint8_t) (checksum + data[index]);
    }

    return checksum;
}

/*
 * 说明：将四位二进制数转换为大写十六进制ASCII字符
 * 输入：value 四位二进制数
 * 输出：uint8_t 十六进制ASCII字符
 */
static uint8_t F_EX201_ValueToHexCharacter(uint8_t value)
{
    uint8_t character = 0U; // 十六进制ASCII字符

    value &= 0x0FU;

    if (value < 10U)
    {
        character = (uint8_t) ('0' + value);
    }
    else
    {
        character = (uint8_t) ('A' + (value - 10U));
    }

    return character;
}

/*
 * 说明：将十六进制ASCII字符转换为四位二进制数
 * 输入：character 十六进制ASCII字符
 *      value     转换结果
 * 输出：bool true表示转换成功，false表示字符无效
 */
static bool F_EX201_HexCharacterToValue(
    uint8_t character,
    uint8_t *value)
{
    if ((character >= (uint8_t) '0') && (character <= (uint8_t) '9'))
    {
        *value = (uint8_t) (character - (uint8_t) '0');
        return true;
    }

    if ((character >= (uint8_t) 'A') && (character <= (uint8_t) 'F'))
    {
        *value = (uint8_t) (character - (uint8_t) 'A' + 10U);
        return true;
    }

    if ((character >= (uint8_t) 'a') && (character <= (uint8_t) 'f'))
    {
        *value = (uint8_t) (character - (uint8_t) 'a' + 10U);
        return true;
    }

    return false;
}

/*
 * 说明：生成EX-201S请求协议帧
 * 输入：address        流量计通信地址
 *      command        四字节命令，不包含字符串结束符
 *      data           请求数据，data_length为0时允许为NULL
 *      data_length    请求数据长度
 *      frame          输出帧缓冲区
 *      frame_capacity 输出帧缓冲区容量
 *      frame_length   实际生成的协议帧长度
 * 输出：F_EX201_ProtocolResult 协议处理结果
 */
F_EX201_ProtocolResult F_EX201_EncodeRequest(
    uint16_t address,
    const char command[EX201_COMMAND_LENGTH],
    const uint8_t *data,
    size_t data_length,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *frame_length)
{
    size_t required_length = 0U; // 当前请求帧所需长度
    size_t frame_index = 0U;     // 输出帧写入位置
    size_t data_index = 0U;      // 请求数据索引
    uint8_t checksum = 0U;       // 请求帧校验和

    if (frame_length != NULL)
    {
        *frame_length = 0U;
    }

    if ((command == NULL) ||
        (frame == NULL) ||
        (frame_length == NULL))
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_ARGUMENT;//输入空指针，返回输入参数无效
    }

    if ((address < EX201_ADDRESS_MIN) ||
        (address > EX201_ADDRESS_MAX) ||
        (data_length > EX201_MAX_DATA_LENGTH) ||
        (!F_EX201_IsDataValid(data, data_length)) ||
        (!F_EX201_IsCommandValid(command)))
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_ARGUMENT;//地址，数据长度不符合要求，返回输入参数无效
    }

    required_length = EX201_REQUEST_FIXED_LENGTH + data_length;//不包含数据的请求帧长度+数据帧的长度

    if (frame_capacity < required_length)
    {
        return F_EX201_PROTOCOL_RESULT_BUFFER_TOO_SMALL;//缓冲区过小
    }

    frame[frame_index++] = EX201_REQUEST_START_CHARACTER;
    frame[frame_index++] = (uint8_t) ('0' + ((address / 100U) % 10U));//将地址百位转换为十进制ASCII字符
    frame[frame_index++] = (uint8_t) ('0' + ((address / 10U) % 10U));//将地址十位转换为十进制ASCII字符
    frame[frame_index++] = (uint8_t) ('0' + (address % 10U));//将地址个位转换为十进制ASCII字符

    for (data_index = 0U; data_index < EX201_COMMAND_LENGTH; data_index++)
    {
        frame[frame_index++] = (uint8_t) command[data_index];
    }

    for (data_index = 0U; data_index < data_length; data_index++)
    {
        frame[frame_index++] = data[data_index];
    }

    checksum = F_EX201_CalculateChecksum(frame, frame_index);
    frame[frame_index++] = F_EX201_ValueToHexCharacter((uint8_t) (checksum >> 4U));
    frame[frame_index++] = F_EX201_ValueToHexCharacter(checksum);
    frame[frame_index++] = EX201_FRAME_END_CHARACTER;

    *frame_length = frame_index;

    return F_EX201_PROTOCOL_RESULT_OK;
}

/*
 * 说明：解析一帧完整的EX-201S响应协议帧
 * 输入：frame        完整响应帧
 *      frame_length 响应帧长度
 *      response     响应解析结果，解析失败时保持原值
 * 输出：F_EX201_ProtocolResult 协议处理结果
 */
F_EX201_ProtocolResult F_EX201_DecodeResponse(
    const uint8_t *frame,
    size_t frame_length,
    F_EX201_Response *response)
{
    F_EX201_Response g_decoded_response = {0}; // 临时解析结果
    size_t command_index = 0U;               // 命令字符索引
    size_t data_index = 0U;                  // 响应数据索引
    size_t data_start_index = 0U;            // 响应数据起始位置
    size_t checksum_index = 0U;              // 校验和字段起始位置
    uint8_t checksum_high = 0U;              // 校验和高四位
    uint8_t checksum_low = 0U;               // 校验和低四位
    uint8_t received_checksum = 0U;          // 报文携带的校验和
    uint8_t calculated_checksum = 0U;        // 本地计算的校验和

    if ((frame == NULL) || (response == NULL))
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_ARGUMENT;//输入参数无效
    }

    if ((frame_length < EX201_RESPONSE_FIXED_LENGTH) ||
        (frame_length > EX201_MAX_RESPONSE_FRAME_LENGTH))
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_FRAME;//协议帧格式无效
    }

    if ((frame[0] != EX201_RESPONSE_START_CHARACTER) ||
        (frame[frame_length - 1U] != EX201_FRAME_END_CHARACTER))
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_FRAME;//协议帧格式无效
    }

    if ((frame[1] < (uint8_t) '0') || (frame[1] > (uint8_t) '9') ||
        (frame[2] < (uint8_t) '0') || (frame[2] > (uint8_t) '9') ||
        (frame[3] < (uint8_t) '0') || (frame[3] > (uint8_t) '9'))
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_FRAME;
    }

    g_decoded_response.address = (uint16_t)
        (((uint16_t) (frame[1] - (uint8_t) '0') * 100U) +
         ((uint16_t) (frame[2] - (uint8_t) '0') * 10U) +
         (uint16_t) (frame[3] - (uint8_t) '0'));

    if ((g_decoded_response.address < EX201_ADDRESS_MIN) ||
        (g_decoded_response.address > EX201_ADDRESS_MAX))
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_FRAME;
    }

    for (command_index = 0U; command_index < EX201_COMMAND_LENGTH; command_index++)
    {
        g_decoded_response.command[command_index] = (char) frame[1U + EX201_ID_LENGTH + command_index];
    }

    g_decoded_response.command[EX201_COMMAND_LENGTH] = '\0';

    if (!F_EX201_IsCommandValid(g_decoded_response.command))
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_FRAME;
    }

    data_start_index =
        1U +
        EX201_ID_LENGTH +
        EX201_COMMAND_LENGTH +
        EX201_RESPONSE_CODE_LENGTH;

    if ((frame[1U + EX201_ID_LENGTH + EX201_COMMAND_LENGTH] == (uint8_t) 'O') &&
        (frame[1U + EX201_ID_LENGTH + EX201_COMMAND_LENGTH + 1U] == (uint8_t) 'K'))
    {
        g_decoded_response.status = F_EX201_RESPONSE_STATUS_OK;
    }
    else if ((frame[1U + EX201_ID_LENGTH + EX201_COMMAND_LENGTH] == (uint8_t) 'N') &&
             (frame[1U + EX201_ID_LENGTH + EX201_COMMAND_LENGTH + 1U] == (uint8_t) 'G'))
    {
        g_decoded_response.status = F_EX201_RESPONSE_STATUS_NG;
    }
    else
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_FRAME;
    }

    checksum_index = frame_length - EX201_CHECKSUM_LENGTH - 1U;
    g_decoded_response.data_length = (uint32_t) (checksum_index - data_start_index);

    if (g_decoded_response.data_length > EX201_MAX_DATA_LENGTH)
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_FRAME;
    }

    if ((!F_EX201_HexCharacterToValue(frame[checksum_index], &checksum_high)) ||
        (!F_EX201_HexCharacterToValue(frame[checksum_index + 1U], &checksum_low)))
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_FRAME;
    }

    received_checksum = (uint8_t) ((checksum_high << 4U) | checksum_low);
    calculated_checksum = F_EX201_CalculateChecksum(frame, checksum_index);

    if (received_checksum != calculated_checksum)
    {
        return F_EX201_PROTOCOL_RESULT_CHECKSUM_ERROR;
    }

    for (data_index = 0U; data_index < g_decoded_response.data_length; data_index++)
    {
        g_decoded_response.data[data_index] = frame[data_start_index + data_index];
    }

    *response = g_decoded_response;//存储解析结果

    return F_EX201_PROTOCOL_RESULT_OK;
}

/*
 * 说明：将流量尾数转换为EX-201S使用的四位十进制ASCII数据
 * 输入：flow_mantissa 流量尾数，范围0000～9999
 *      p_data        输出的四字节ASCII数据
 * 输出：F_EX201_ProtocolResult 转换结果
 */
F_EX201_ProtocolResult F_EX201_EncodeFlowValue(
    uint16_t flow_mantissa,
    uint8_t p_data[EX201_FLOW_MANTISSA_LENGTH])
{
    if ((p_data == NULL) || (flow_mantissa > EX201_FLOW_MANTISSA_MAX))
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_ARGUMENT;
    }

    p_data[0] = (uint8_t) ('0' + ((flow_mantissa / 1000U) % 10U)); // 流量尾数千位
    p_data[1] = (uint8_t) ('0' + ((flow_mantissa / 100U) % 10U));  // 流量尾数百位
    p_data[2] = (uint8_t) ('0' + ((flow_mantissa / 10U) % 10U));   // 流量尾数十位
    p_data[3] = (uint8_t) ('0' + (flow_mantissa % 10U));           // 流量尾数个位

    return F_EX201_PROTOCOL_RESULT_OK;
}

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
    uint32_t *p_value)
{
    size_t data_index = 0U; // 当前十进制字符索引
    uint32_t value = 0U;    // 已解析的无符号整数

    if ((p_data == NULL) || (p_value == NULL))
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_ARGUMENT;
    }

    if ((0U == data_length) || (data_length > EX201_FLOW_MANTISSA_LENGTH))
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_FRAME;
    }

    for (data_index = 0U; data_index < data_length; data_index++)
    {
        if ((p_data[data_index] < (uint8_t) '0') ||
            (p_data[data_index] > (uint8_t) '9'))
        {
            return F_EX201_PROTOCOL_RESULT_INVALID_FRAME;
        }

        value =
            (value * 10U) +
            (uint32_t) (p_data[data_index] - (uint8_t) '0');
    }

    *p_value = value;

    return F_EX201_PROTOCOL_RESULT_OK;
}

/*
 * 说明：将EX-201S流量ASCII数据转换为有符号流量尾数
 * 输入：p_data          四位无符号数据或符号加四位瞬时流量数据
 *      data_length     数据长度
 *      p_flow_mantissa 输出的有符号流量尾数
 * 输出：F_EX201_ProtocolResult 转换结果
 */
F_EX201_ProtocolResult F_EX201_DecodeFlowValue(
    const uint8_t *p_data,
    size_t data_length,
    int32_t *p_flow_mantissa)
{
    F_EX201_ProtocolResult decode_result = F_EX201_PROTOCOL_RESULT_OK; // 无符号尾数解析结果
    size_t digit_start = 0U;                                         // 十进制数字起始位置
    uint32_t absolute_value = 0U;                                    // 流量尾数绝对值
    int32_t sign = 1;                                                // 流量尾数符号

    if ((p_data == NULL) || (p_flow_mantissa == NULL))
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_ARGUMENT;
    }

    if (EX201_SIGNED_FLOW_LENGTH == data_length)
    {
        if ((uint8_t) '+' == p_data[0])
        {
            sign = 1;
        }
        else if ((uint8_t) '-' == p_data[0])
        {
            sign = -1;
        }
        else
        {
            return F_EX201_PROTOCOL_RESULT_INVALID_FRAME;
        }

        digit_start = 1U;
    }
    else if (EX201_FLOW_MANTISSA_LENGTH != data_length)
    {
        return F_EX201_PROTOCOL_RESULT_INVALID_FRAME;
    }

    decode_result = F_EX201_DecodeUnsignedValue(
        &p_data[digit_start],
        EX201_FLOW_MANTISSA_LENGTH,
        &absolute_value);

    if (F_EX201_PROTOCOL_RESULT_OK != decode_result)
    {
        return decode_result;
    }

    *p_flow_mantissa = (int32_t) absolute_value * sign;

    return F_EX201_PROTOCOL_RESULT_OK;
}

/*
 * 说明：将硬件层结果转换为EX-201S功能层传输结果
 * 输入：hardware_result 硬件层处理结果
 * 输出：F_EX201_TransportResult 功能层传输结果
 */
static F_EX201_TransportResult F_EX201_MapHardwareResult(H_EX201_Result hardware_result)
{
    F_EX201_TransportResult transport_result = F_EX201_TRANSPORT_RESULT_DRIVER_ERROR; // 功能层传输结果

    switch (hardware_result)
    {
        case H_EX201_RESULT_OK:
            transport_result = F_EX201_TRANSPORT_RESULT_OK;
            break;

        case H_EX201_RESULT_INVALID_ARGUMENT:
            transport_result = F_EX201_TRANSPORT_RESULT_INVALID_ARGUMENT;
            break;

        case H_EX201_RESULT_NOT_INITIALIZED:
            transport_result = F_EX201_TRANSPORT_RESULT_NOT_INITIALIZED;
            break;

        case H_EX201_RESULT_BUSY:
            transport_result = F_EX201_TRANSPORT_RESULT_BUSY;
            break;

        case H_EX201_RESULT_NO_DATA:
            transport_result = F_EX201_TRANSPORT_RESULT_NO_FRAME;
            break;

        case H_EX201_RESULT_RECEIVE_OVERFLOW:
            transport_result = F_EX201_TRANSPORT_RESULT_RECEIVE_OVERFLOW;
            break;

        case H_EX201_RESULT_UART_ERROR:
            transport_result = F_EX201_TRANSPORT_RESULT_UART_ERROR;
            break;

        case H_EX201_RESULT_DRIVER_ERROR:
        default:
            transport_result = F_EX201_TRANSPORT_RESULT_DRIVER_ERROR;
            break;
    }

    return transport_result;
}

/*
 * 说明：初始化EX-201S功能层和RS485硬件层
 * 输入：p_context 功能模块上下文
 * 输出：F_EX201_TransportResult 初始化结果
 */
F_EX201_TransportResult F_EX201_Initialize(F_EX201_Context *p_context)
{
    H_EX201_Result hardware_result = H_EX201_RESULT_OK; // 硬件层初始化结果

    if (p_context == NULL)
    {
        return F_EX201_TRANSPORT_RESULT_INVALID_ARGUMENT;
    }

    p_context->response_length = 0U;
    p_context->initialized = 0U;

    hardware_result = H_EX201_Initialize(&p_context->hardware_context);

    if (H_EX201_RESULT_OK != hardware_result)
    {
        return F_EX201_MapHardwareResult(hardware_result);
    }

    p_context->initialized = 1U;

    return F_EX201_TRANSPORT_RESULT_OK;
}

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
    size_t data_length)
{
    H_EX201_Result hardware_result = H_EX201_RESULT_OK; // 硬件层发送结果

    if ((p_context == NULL) || (p_data == NULL) || (0U == data_length))
    {
        return F_EX201_TRANSPORT_RESULT_INVALID_ARGUMENT;
    }

    if (0U == p_context->initialized)
    {
        return F_EX201_TRANSPORT_RESULT_NOT_INITIALIZED;
    }

    p_context->response_length = 0U;
    hardware_result = H_EX201_Send(
        &p_context->hardware_context,
        p_data,
        data_length);

    return F_EX201_MapHardwareResult(hardware_result);
}

/*
 * 说明：从硬件层取字节并组装一帧完整EX-201S响应
 * 输入：p_context      功能模块上下文
 *      p_frame        输出帧缓冲区
 *      frame_capacity 输出帧缓冲区容量
 *      p_frame_length 实际响应帧长度
 * 输出：F_EX201_TransportResult 接收结果
 */
F_EX201_TransportResult F_EX201_ReceiveFrame(
    F_EX201_Context *p_context,
    uint8_t *p_frame,
    size_t frame_capacity,
    size_t *p_frame_length)
{
    H_EX201_Result hardware_result = H_EX201_RESULT_OK; // 硬件层读取结果
    uint8_t received_data = 0U;                         // 当前读取的接收字节
    size_t frame_index = 0U;                            // 输出帧复制索引

    if (p_frame_length != NULL)
    {
        *p_frame_length = 0U;
    }

    if ((p_context == NULL) ||
        (p_frame == NULL) ||
        (p_frame_length == NULL))
    {
        return F_EX201_TRANSPORT_RESULT_INVALID_ARGUMENT;
    }

    if (0U == p_context->initialized)
    {
        return F_EX201_TRANSPORT_RESULT_NOT_INITIALIZED;
    }

    while (true)
    {
        hardware_result = H_EX201_ReceiveByte(
            &p_context->hardware_context,
            &received_data);

        if (H_EX201_RESULT_NO_DATA == hardware_result)
        {
            return F_EX201_TRANSPORT_RESULT_NO_FRAME;
        }

        if (H_EX201_RESULT_OK != hardware_result)
        {
            p_context->response_length = 0U;
            return F_EX201_MapHardwareResult(hardware_result);
        }

        if ((0U == p_context->response_length) &&
            (EX201_RESPONSE_START_CHARACTER != received_data))
        {
            continue;
        }

        if (p_context->response_length >= EX201_MAX_RESPONSE_FRAME_LENGTH)
        {
            p_context->response_length = 0U;
            H_EX201_ResetReceiver(&p_context->hardware_context);
            return F_EX201_TRANSPORT_RESULT_RECEIVE_OVERFLOW;
        }

        p_context->response_frame[p_context->response_length] = received_data;
        p_context->response_length++;

        if (EX201_FRAME_END_CHARACTER != received_data)
        {
            continue;
        }

        *p_frame_length = (size_t) p_context->response_length;

        if (frame_capacity < *p_frame_length)
        {
            p_context->response_length = 0U;
            return F_EX201_TRANSPORT_RESULT_BUFFER_TOO_SMALL;
        }

        for (frame_index = 0U; frame_index < *p_frame_length; frame_index++)
        {
            p_frame[frame_index] = p_context->response_frame[frame_index];
        }

        p_context->response_length = 0U;
        return F_EX201_TRANSPORT_RESULT_OK;
    }
}

/*
 * 说明：通过硬件层恢复异常的RS485通信
 * 输入：p_context 功能模块上下文
 * 输出：F_EX201_TransportResult 恢复结果
 */
F_EX201_TransportResult F_EX201_Recover(F_EX201_Context *p_context)
{
    H_EX201_Result hardware_result = H_EX201_RESULT_OK; // 硬件层恢复结果

    if (p_context == NULL)
    {
        return F_EX201_TRANSPORT_RESULT_INVALID_ARGUMENT;
    }

    p_context->response_length = 0U;
    hardware_result = H_EX201_Recover(&p_context->hardware_context);

    return F_EX201_MapHardwareResult(hardware_result);
}

/*
 * 说明：查询硬件层是否仍在发送EX-201S数据
 * 输入：p_context 功能模块上下文
 * 输出：uint32_t 非0表示正在发送，0表示发送空闲
 */
uint32_t F_EX201_IsTransmitBusy(F_EX201_Context *p_context)
{
    if (p_context == NULL)
    {
        return 0U;
    }

    return H_EX201_IsTransmitBusy(&p_context->hardware_context);
}

/*
 * 说明：清空EX-201S功能层和硬件层接收状态
 * 输入：p_context 功能模块上下文
 * 输出：无
 */
void F_EX201_ResetReceiver(F_EX201_Context *p_context)
{
    if (p_context == NULL)
    {
        return;
    }

    p_context->response_length = 0U;
    H_EX201_ResetReceiver(&p_context->hardware_context);
}
