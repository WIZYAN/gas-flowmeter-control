/*
 * F_CanUser.c
 * Created on: 2026年9月14日
 * Author: YXZ
 * 协议来源：E:/c/can_module/F_Can_protocol.c、H_Can_underlying_fun.c。
 */
#include "F_CanUser.h"
#include <stddef.h>
#include <string.h>
#include <float.h>

typedef char F_CanUser_FloatSizeCheck[(sizeof(float) == 4U) ? 1 : -1]; // 要求32位float
#if (FLT_RADIX != 2) || (FLT_MANT_DIG != 24) || (FLT_MAX_EXP != 128)
#error "CAN_USER requires IEEE-754 binary32 float."
#endif

/*
 * 说明：保留原协议CRC16累积及截取规则
 * 输入：p_frame 输入帧
 * 输出：uint8_t 校验字节
 */
uint8_t F_CanUser_Checksum(const F_CanUser_Frame *p_frame)
{
    uint16_t crc = 0xFFFFU; // 原算法初始值
    uint32_t index = 0U;   // 校验输入索引
    uint32_t bit = 0U;     // 位循环索引
    uint8_t value = 0U;    // 当前参与计算的字节
    if (NULL == p_frame)
    {
        return 0U;
    }
    for (index = 0U; index < 9U; index++)
    {
        if (2U == index)
        {
            continue;
        }
        value = (index < 8U) ? p_frame->data[index] : (uint8_t) (p_frame->id >> 24U);
        crc = (uint16_t) (crc ^ value);
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = (uint16_t) ((crc >> 1U) ^ ((0U != (crc & 1U)) ? 0xA001U : 0U));
        }
    }
    return (uint8_t) (crc >> 4U);
}

/*
 * 说明：生成与原工程一致的扩展ID及8字节电文
 * 输入：p_message 报文，p_frame 输出帧
 * 输出：F_CanUser_Result 编码结果
 */
F_CanUser_Result F_CanUser_Encode(const F_CanUser_Message *p_message, F_CanUser_Frame *p_frame)
{
    uint32_t index = 0U; // 负载字节索引
    if ((NULL == p_message) || (NULL == p_frame) || (p_message->function > 31U) ||
        (p_message->target_type > 31U) || (p_message->source_type > 31U) ||
        (p_message->target_address > 127U) || (p_message->source_address > 127U))
    {
        return F_CANUSER_RESULT_ARGUMENT;
    }
    p_frame->id = ((uint32_t) p_message->function << 24U) |
                  ((uint32_t) p_message->target_type << 19U) |
                  ((uint32_t) p_message->target_address << 12U) |
                  ((uint32_t) p_message->source_type << 7U) | p_message->source_address;
    p_frame->data[0] = (uint8_t) p_message->address;
    p_frame->data[1] = (uint8_t) (p_message->address >> 8U);
    p_frame->data[3] = p_message->count;
    for (index = 0U; index < 4U; index++)
    {
        p_frame->data[index + 4U] = (uint8_t) (p_message->value >> (index * 8U));
    }
    p_frame->data[2] = F_CanUser_Checksum(p_frame);
    return F_CANUSER_RESULT_OK;
}

/*
 * 说明：按原协议解包，使用显式小端读取避免未对齐访问
 * 输入：p_frame 输入帧，p_message 输出报文
 * 输出：F_CanUser_Result 解码结果
 */
F_CanUser_Result F_CanUser_Decode(const F_CanUser_Frame *p_frame, F_CanUser_Message *p_message)
{
    F_CanUser_Message g_message = {0}; // 校验通过后一次提交的报文
    uint32_t index = 0U;              // 数据字节索引
    if ((NULL == p_frame) || (NULL == p_message) || (p_frame->id > F_CANUSER_ID_MAX))
    {
        return F_CANUSER_RESULT_ARGUMENT;
    }
    if (p_frame->data[2] != F_CanUser_Checksum(p_frame))
    {
        return F_CANUSER_RESULT_CHECKSUM;
    }
    g_message.source_address = (uint8_t) (p_frame->id & 0x7FU);
    g_message.source_type = (uint8_t) ((p_frame->id >> 7U) & 0x1FU);
    g_message.target_address = (uint8_t) ((p_frame->id >> 12U) & 0x7FU);
    g_message.target_type = (uint8_t) ((p_frame->id >> 19U) & 0x1FU);
    g_message.function = (uint8_t) ((p_frame->id >> 24U) & 0x1FU);
    g_message.address = (uint16_t) ((uint16_t) p_frame->data[0] | ((uint16_t) p_frame->data[1] << 8U));
    g_message.count = p_frame->data[3];
    for (index = 0U; index < 4U; index++)
    {
        g_message.value |= (uint32_t) p_frame->data[index + 4U] << (index * 8U);
    }
    *p_message = g_message;
    return F_CANUSER_RESULT_OK;
}

/*
 * 说明：判断参数区间，包含每段最后一个地址
 * 输入：address 参数地址
 * 输出：F_CanUser_DataKind 参数类别
 */
F_CanUser_DataKind F_CanUser_GetDataKind(uint16_t address)
{
    if (address <= 0x00FFU)
    {
        return F_CANUSER_KIND_READ_FLOAT;
    }
    if (address <= 0x01FFU)
    {
        return F_CANUSER_KIND_READ_UINT;
    }
    if (address <= 0x02FFU)
    {
        return F_CANUSER_KIND_WRITE_FLOAT;
    }
    if (address <= 0x03FFU)
    {
        return F_CANUSER_KIND_WRITE_UINT;
    }
    return F_CANUSER_KIND_UNDEFINED;
}

/*
 * 说明：按位保存float32
 * 输入：value 浮点值
 * 输出：uint32_t 位模式
 */
uint32_t F_CanUser_FloatToBits(float value)
{
    uint32_t bits = 0U; // 浮点位模式
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

/*
 * 说明：按位还原float32
 * 输入：value 位模式
 * 输出：float 浮点值
 */
float F_CanUser_BitsToFloat(uint32_t value)
{
    float result = 0.0F; // 解码浮点值
    memcpy(&result, &value, sizeof(result));
    return result;
}
