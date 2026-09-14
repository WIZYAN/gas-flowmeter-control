/*
 * F_CanUser.h
 * Created on: 2026年9月14日
 * Author: YXZ
 */
#ifndef CAN_F_CAN_USER_H_
#define CAN_F_CAN_USER_H_

#include <stdint.h>

#define F_CANUSER_DATA_LENGTH (8U) // CAN_USER固定数据长度
#define F_CANUSER_ID_MAX (0x1FFFFFFFUL) // 29位扩展标识符上限

typedef enum
{
    Type_Header = 0x00, // 上位机类型，沿用原协议
    Type_FLOW = 0x0F    // 流量控制主板类型，沿用原协议
} F_CanUser_NodeType;

typedef enum
{
    F_CANUSER_WRITE = 0x01,           // 原Fun_WriteCode
    F_CANUSER_READ = 0x02,            // 原Fun_ReadCode
    F_CANUSER_BROADCAST_WRITE = 0x03, // 原Fun_BroadcastWriteCode
    F_CANUSER_BROADCAST_READ = 0x04,  // 原Fun_BroadcastReadCode
    F_CANUSER_CYCLE_OUTPUT = 0x05,    // 原Fun_CycleOutputCode
    F_CANUSER_WRITE_RETURN = 0x06,    // 原Fun_WriteReturnCode
    F_CANUSER_READ_RETURN = 0x07,     // 原Fun_ReadReturnCode
    F_CANUSER_HEARTBEAT = 0x08        // 原Fun_HeartBeatCode
} F_CanUser_Function;

typedef enum
{
    F_CANUSER_KIND_READ_FLOAT = 0, // 0x0000～0x00FF
    F_CANUSER_KIND_READ_UINT,      // 0x0100～0x01FF
    F_CANUSER_KIND_WRITE_FLOAT,    // 0x0200～0x02FF，可读写
    F_CANUSER_KIND_WRITE_UINT,     // 0x0300～0x03FF，可读写
    F_CANUSER_KIND_UNDEFINED       // 当前产品未定义的地址区
} F_CanUser_DataKind;

typedef enum
{
    F_CANUSER_RESULT_OK = 0,       // 编解码成功
    F_CANUSER_RESULT_ARGUMENT,    // 空指针或字段越界
    F_CANUSER_RESULT_CHECKSUM     // 自定义校验字节不匹配
} F_CanUser_Result;

typedef struct
{
    uint32_t id;                            // 29位扩展CAN ID
    uint8_t data[F_CANUSER_DATA_LENGTH];    // 原协议8字节负载
} F_CanUser_Frame;

typedef struct
{
    uint32_t value;          // D4～D7的小端位模式，浮点数也先保留原始位
    uint16_t address;        // D0、D1参数地址
    uint8_t count;           // D3参数个数
    uint8_t function;        // ID位24～28功能码
    uint8_t target_type;     // ID位19～23目标类型
    uint8_t target_address;  // ID位12～18目标节点地址
    uint8_t source_type;     // ID位7～11来源类型
    uint8_t source_address;  // ID位0～6来源节点地址
} F_CanUser_Message;

/*
 * 说明：计算原H_can_crc_code_fun定义的校验字节
 * 输入：p_frame CAN帧；D2不参与计算
 * 输出：uint8_t CRC16右移4位后截取的8位值
 */
uint8_t F_CanUser_Checksum(const F_CanUser_Frame *p_frame);

/*
 * 说明：按原CAN_USER布局编码一帧，不使用编译器位域
 * 输入：p_message 逻辑报文，p_frame 输出帧
 * 输出：F_CanUser_Result 编码结果
 */
F_CanUser_Result F_CanUser_Encode(const F_CanUser_Message *p_message, F_CanUser_Frame *p_frame);

/*
 * 说明：校验并解析CAN_USER帧，失败时不修改输出
 * 输入：p_frame 输入帧，p_message 输出报文
 * 输出：F_CanUser_Result 解码结果
 */
F_CanUser_Result F_CanUser_Decode(const F_CanUser_Frame *p_frame, F_CanUser_Message *p_message);

/*
 * 说明：按用户规定的四个完整地址区间判断数据类型
 * 输入：address 参数地址
 * 输出：F_CanUser_DataKind 类型及权限类别
 */
F_CanUser_DataKind F_CanUser_GetDataKind(uint16_t address);

/*
 * 说明：提取float32原始位模式，不进行数值强制转换
 * 输入：value 浮点数
 * 输出：uint32_t 原始32位数据
 */
uint32_t F_CanUser_FloatToBits(float value);

/*
 * 说明：将32位原始数据解释为float32
 * 输入：value 原始位模式
 * 输出：float 浮点数
 */
float F_CanUser_BitsToFloat(uint32_t value);

#endif
