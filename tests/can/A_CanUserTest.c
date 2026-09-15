#include "A_CanUserTest.h"
#include "A_HostCan.h"
#include "H_CanMock.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/*
 * 说明：沿用用户H_can_crc_code_fun算法作为兼容性参考
 * 输入：p_frame 帧
 * 输出：uint8_t 原算法校验值
 */
static uint8_t A_CanUserTest_LegacyChecksum(const F_CanUser_Frame *p_frame)
{
    uint8_t data[8]={0}; // 原算法输入顺序
    uint16_t crc=0xFFFFU; // 原算法初始值
    uint32_t i=0U, j=0U; // 循环索引
    for(i=0U;i<2U;i++) { data[i]=p_frame->data[i]; }
    for(i=3U;i<8U;i++) { data[i-1U]=p_frame->data[i]; }
    data[7]=(uint8_t)(p_frame->id>>24U);
    for(i=0U;i<8U;i++)
    {
        crc=(uint16_t)(crc^data[i]);
        for(j=0U;j<8U;j++)
        {
            if(crc&1U) { crc>>=1U; crc^=0xA001U; }
            else { crc>>=1U; }
        }
    }
    return (uint8_t)(crc>>4U);
}
/*
 * 说明：初始化一次独立测试
 * 输入：p_context 被测上下文
 * 输出：无
 */
static void A_CanUserTest_Reset(A_HostCan_Context *p_context)
{
    H_CanMock_Reset();
    memset(p_context,0,sizeof(*p_context));
    assert(A_HostCan_Initialize(p_context,1U));
}
/*
 * 说明：通过真实H层回调注入上位机请求
 * 输入：function 功能，address 参数，count 数量，value 位模式，source 来源地址
 * 输出：无
 */
static void A_CanUserTest_Inject(uint8_t function,uint16_t address,uint8_t count,uint32_t value,uint8_t source)
{
    F_CanUser_Message g_message={0}; // 请求
    F_CanUser_Frame g_protocol={0};  // 协议帧
    can_frame_t g_frame={0};        // FSP帧
    g_message.function=function; g_message.address=address; g_message.count=count; g_message.value=value;
    g_message.source_type=Type_Header; g_message.source_address=source;
    g_message.target_type=Type_FLOW; g_message.target_address=1U;
    assert(F_CanUser_Encode(&g_message,&g_protocol)==F_CANUSER_RESULT_OK);
    g_frame.id=g_protocol.id; g_frame.id_mode=CAN_ID_MODE_EXTENDED;
    g_frame.type=CAN_FRAME_TYPE_DATA; g_frame.data_length_code=8U;
    memcpy(g_frame.data,g_protocol.data,8U);
    H_CanMock_Inject(&g_frame);
}
/*
 * 说明：推进若干个1ms任务周期
 * 输入：p_context 上下文，start 起始毫秒，count 周期数
 * 输出：无
 */
static void A_CanUserTest_Pump(A_HostCan_Context *p_context,uint32_t start,uint32_t count)
{
    uint32_t i=0U; // 周期索引
    for(i=0U;i<count;i++) { A_HostCan_Process(p_context,start+i); }
}
/*
 * 说明：校验一条期望回复
 * 输入：function 功能，address 地址，value 位模式，target 目标节点
 * 输出：无
 */
static void A_CanUserTest_Expect(uint8_t function,uint16_t address,uint32_t value,uint8_t target)
{
    can_frame_t g_frame={0};        // 捕获的发送帧
    F_CanUser_Frame g_protocol={0}; // 原始协议帧
    F_CanUser_Message g_message={0}; // 解码结果
    assert(H_CanMock_Pop(&g_frame));
    g_protocol.id=g_frame.id; memcpy(g_protocol.data,g_frame.data,8U);
    assert(F_CanUser_Decode(&g_protocol,&g_message)==F_CANUSER_RESULT_OK);
    assert(g_message.function==function && g_message.address==address && g_message.value==value);
    assert(g_message.source_type==Type_FLOW && g_message.source_address==1U);
    assert(g_message.target_type==Type_Header && g_message.target_address==target && g_message.count==1U);
}
/*
 * 说明：确认没有提前或额外回复
 * 输入：无
 * 输出：无
 */
static void A_CanUserTest_Empty(void)
{
    can_frame_t g_frame={0}; // 捕获帧
    assert(!H_CanMock_Pop(&g_frame));
}
/*
 * 说明：建立已就绪的MFC通道快照
 * 输入：p_context 上下文
 * 输出：无
 */
static void A_CanUserTest_Channels(A_HostCan_Context *p_context)
{
    A_HostCan_Channel g_channel={0}; // 测试设备数据
    uint32_t i=0U; // 通道索引
    g_channel.valid_flags=0x3FFU; g_channel.online=1U; g_channel.initialize_state=2U;
    g_channel.full_scale=100.0F; g_channel.sampled_ms=10U; g_channel.decimal_places=1U;
    for(i=0U;i<6U;i++)
    {
        g_channel.actual_flow=(float)i+1.0F; g_channel.device_address=i+1U;
        assert(A_HostCan_PublishChannel(p_context,i,&g_channel));
    }
}
/*
 * 说明：验证原协议ID布局、校验、大小端及完整区间边界
 * 输入：无
 * 输出：无
 */
static void A_CanUserTest_Codec(void)
{
    F_CanUser_Message g_message={0}; // 输入报文
    F_CanUser_Message g_decoded={0}; // 解码报文
    F_CanUser_Frame g_frame={0};     // 帧
    uint32_t i=0U; // 测试向量索引
    g_message.function=1U; g_message.target_type=15U; g_message.target_address=1U;
    g_message.source_address=1U; g_message.address=0x0202U; g_message.count=1U;
    g_message.value=0x42C80000UL;
    assert(F_CanUser_Encode(&g_message,&g_frame)==F_CANUSER_RESULT_OK);
    assert(g_frame.id==0x01781001UL);
    assert(g_frame.data[0]==2U && g_frame.data[1]==2U && g_frame.data[3]==1U);
    assert(g_frame.data[4]==0U && g_frame.data[5]==0U && g_frame.data[6]==0xC8U && g_frame.data[7]==0x42U);
    assert(F_CanUser_FloatToBits(100.0F)==0x42C80000UL);
    for(i=0U;i<1024U;i++)
    {
        g_message.function=(uint8_t)(i%32U);
        g_message.address=(uint16_t)(i*61U);
        g_message.count=(uint8_t)i; g_message.value=0xF1234567UL^i;
        g_message.target_address=(uint8_t)(i%128U); g_message.source_address=(uint8_t)((i+7U)%128U);
        g_message.source_type=(uint8_t)((i+3U)%32U);
        assert(F_CanUser_Encode(&g_message,&g_frame)==F_CANUSER_RESULT_OK);
        assert(g_frame.data[2]==A_CanUserTest_LegacyChecksum(&g_frame));
        assert(F_CanUser_Decode(&g_frame,&g_decoded)==F_CANUSER_RESULT_OK);
        assert(g_decoded.value==g_message.value && g_decoded.address==g_message.address);
        assert(g_decoded.source_type==g_message.source_type && g_decoded.source_address==g_message.source_address);
        g_frame.data[2]^=1U; g_decoded.value=0xDEADBEEFUL;
        assert(F_CanUser_Decode(&g_frame,&g_decoded)==F_CANUSER_RESULT_CHECKSUM);
        assert(g_decoded.value==0xDEADBEEFUL);
    }
    assert(F_CanUser_GetDataKind(0x0000U)==F_CANUSER_KIND_READ_FLOAT);
    assert(F_CanUser_GetDataKind(0x00FFU)==F_CANUSER_KIND_READ_FLOAT);
    assert(F_CanUser_GetDataKind(0x0100U)==F_CANUSER_KIND_READ_UINT);
    assert(F_CanUser_GetDataKind(0x01FFU)==F_CANUSER_KIND_READ_UINT);
    assert(F_CanUser_GetDataKind(0x0200U)==F_CANUSER_KIND_WRITE_FLOAT);
    assert(F_CanUser_GetDataKind(0x02FFU)==F_CANUSER_KIND_WRITE_FLOAT);
    assert(F_CanUser_GetDataKind(0x0300U)==F_CANUSER_KIND_WRITE_UINT);
    assert(F_CanUser_GetDataKind(0x03FFU)==F_CANUSER_KIND_WRITE_UINT);
    assert(F_CanUser_GetDataKind(0x0400U)==F_CANUSER_KIND_UNDEFINED);
    assert(F_CanUser_GetDataKind(0xFFFFU)==F_CANUSER_KIND_UNDEFINED);
    assert(F_CanUser_Encode(NULL,&g_frame)==F_CANUSER_RESULT_ARGUMENT);
    g_frame.id=0x20000000UL;
    assert(F_CanUser_Decode(&g_frame,&g_decoded)==F_CANUSER_RESULT_ARGUMENT);
}
/*
 * 说明：验证硬件中断过滤、队列溢出、顺序及错误事件
 * 输入：无
 * 输出：无
 */
static void A_CanUserTest_Hardware(void)
{
    H_HostCan_Context g_hardware={0}; // 硬件上下文
    H_HostCan_Frame g_output={0};     // 接收帧
    can_frame_t g_frame={0};          // FSP帧
    uint32_t i=0U; // 帧索引
    H_CanMock_Reset(); assert(H_HostCan_Initialize(&g_hardware)==H_HOSTCAN_OK);
    g_frame.id_mode=CAN_ID_MODE_STANDARD; g_frame.data_length_code=8U; H_CanMock_Inject(&g_frame);
    g_frame.id_mode=CAN_ID_MODE_EXTENDED; g_frame.type=CAN_FRAME_TYPE_REMOTE; H_CanMock_Inject(&g_frame);
    g_frame.type=CAN_FRAME_TYPE_DATA; g_frame.data_length_code=7U; H_CanMock_Inject(&g_frame);
    assert(g_hardware.dropped_frames==3U);
    g_frame.data_length_code=8U;
    for(i=0U;i<17U;i++) { g_frame.id=i; H_CanMock_Inject(&g_frame); }
    assert(g_hardware.dropped_frames==4U && g_hardware.receive_count==16U);
    for(i=0U;i<16U;i++) { assert(H_HostCan_Receive(&g_hardware,&g_output)==H_HOSTCAN_OK); assert(g_output.id==i); }
    assert(H_HostCan_Receive(&g_hardware,&g_output)==H_HOSTCAN_EMPTY);
    H_CanMock_Event(CAN_EVENT_ERR_BUS_OFF);
    assert(H_HostCan_GetTransmitState(&g_hardware)==H_HOSTCAN_ERROR);
    assert(H_HostCan_Recover(&g_hardware)==H_HOSTCAN_OK);
}
/*
 * 说明：验证参数查询、异步写回复、原请求关联、阀门联锁及异常恢复
 * 输入：无
 * 输出：无
 */
static void A_CanUserTest_Application(void)
{
    A_HostCan_Context g_context={0}; // 业务上下文
    A_HostCan_Command g_command={0}; // 交给执行器的请求
    A_HostCan_System g_system={0};   // 整机快照
    uint32_t i=0U; // 循环索引
    uint32_t sequence=0U; // 请求编号

    A_CanUserTest_Reset(&g_context);
    A_CanUserTest_Inject(1U,0x0200U,1U,F_CanUser_FloatToBits(10.0F),1U);
    A_CanUserTest_Pump(&g_context,1U,4U);
    A_CanUserTest_Expect(6U,0x0200U,A_HOSTCAN_CODE_NOT_READY,1U);
    A_CanUserTest_Inject(2U,0x0100U,1U,0U,1U);
    A_CanUserTest_Pump(&g_context,5U,4U);
    A_CanUserTest_Expect(7U,0x0100U,0U,1U);
    A_CanUserTest_Channels(&g_context);
    g_system.faults=0xF1234567UL;
    assert(A_HostCan_PublishSystem(&g_context,&g_system));
    A_CanUserTest_Inject(2U,0x0101U,1U,0U,1U);
    A_CanUserTest_Pump(&g_context,10U,4U);
    A_CanUserTest_Expect(7U,0x0101U,0xF1234567UL,1U);
    A_CanUserTest_Inject(2U,0U,6U,0U,1U);
    A_CanUserTest_Pump(&g_context,15U,10U);
    for(i=0U;i<6U;i++) { A_CanUserTest_Expect(7U,(uint16_t)i,F_CanUser_FloatToBits((float)i+1.0F),1U); }
    A_CanUserTest_Empty();

    A_HostCan_SetExecutors(&g_context,A_HOSTCAN_EXECUTOR_MFC);
    A_CanUserTest_Inject(1U,0x0202U,1U,F_CanUser_FloatToBits(12.5F),2U);
    A_CanUserTest_Pump(&g_context,30U,3U);
    A_CanUserTest_Empty(); // 入队没有提前回复OK
    assert(A_HostCan_TakeCommand(&g_context,&g_command));
    assert(!A_HostCan_TakeCommand(&g_context,&g_command));
    assert(g_command.index==2U && g_command.value==0x41480000UL);
    sequence=g_command.sequence;
    A_CanUserTest_Inject(2U,0x0100U,1U,0U,1U);
    A_CanUserTest_Inject(1U,0x0200U,1U,F_CanUser_FloatToBits(1.0F),1U);
    A_CanUserTest_Pump(&g_context,35U,5U);
    A_CanUserTest_Expect(7U,0x0100U,0U,1U);
    A_CanUserTest_Expect(6U,0x0200U,A_HOSTCAN_CODE_BUSY,1U);
    assert(!A_HostCan_CompleteCommand(&g_context,sequence+1U,0U,40U));
    assert(A_HostCan_CompleteCommand(&g_context,sequence,0U,40U));
    A_CanUserTest_Pump(&g_context,40U,4U);
    A_CanUserTest_Expect(6U,0x0202U,0U,2U); // 后续请求未覆盖原主机地址
    assert(!A_HostCan_CompleteCommand(&g_context,sequence,0U,44U));

    A_CanUserTest_Inject(1U,0x0200U,1U,0x7FC00000UL,1U);
    A_CanUserTest_Inject(1U,0x0200U,1U,F_CanUser_FloatToBits(101.0F),1U);
    A_CanUserTest_Inject(1U,0x0100U,1U,1U,1U);
    A_CanUserTest_Inject(1U,0x02FFU,1U,1U,1U);
    A_CanUserTest_Pump(&g_context,50U,8U);
    A_CanUserTest_Expect(6U,0x0200U,A_HOSTCAN_CODE_VALUE,1U);
    A_CanUserTest_Expect(6U,0x0200U,A_HOSTCAN_CODE_RANGE,1U);
    A_CanUserTest_Expect(6U,0x0100U,A_HOSTCAN_CODE_READ_ONLY,1U);
    A_CanUserTest_Expect(6U,0x02FFU,A_HOSTCAN_CODE_ADDRESS,1U);
    A_CanUserTest_Inject(2U,0x0005U,2U,0U,1U); // 跨入预留地址，整个读请求无数据回复
    A_CanUserTest_Pump(&g_context,60U,4U);
    A_CanUserTest_Empty();
    A_CanUserTest_Inject(2U,0x010CU,3U,0U,1U);
    A_CanUserTest_Pump(&g_context,65U,6U);
    A_CanUserTest_Expect(7U,0x010CU,6U,1U);
    A_CanUserTest_Expect(7U,0x010DU,A_HOSTCAN_CODE_ADDRESS,1U);
    A_CanUserTest_Expect(7U,0x010EU,2U,1U);

    g_system.valves_valid=1U; g_system.valve_outputs=A_HOSTCAN_VALVE8; g_system.valve_target=A_HOSTCAN_VALVE8;
    assert(A_HostCan_PublishSystem(&g_context,&g_system));
    A_HostCan_SetExecutors(&g_context,A_HOSTCAN_EXECUTOR_VALVE);
    A_CanUserTest_Inject(1U,0x0306U,1U,1U,1U);
    A_CanUserTest_Pump(&g_context,80U,3U);
    assert(A_HostCan_TakeCommand(&g_context,&g_command));
    assert(g_command.valve_target==A_HOSTCAN_VALVE_GROUP79);
    assert(g_context.system.valve_outputs==A_HOSTCAN_VALVE8); // CAN层没有操作GPIO
    assert(A_HostCan_CompleteCommand(&g_context,g_command.sequence,0x42U,83U));
    A_CanUserTest_Pump(&g_context,83U,4U);
    A_CanUserTest_Expect(6U,0x0306U,0x42U,1U); // 自定义业务码
    A_CanUserTest_Inject(1U,0x0309U,1U,0x0040U,1U);
    A_CanUserTest_Inject(1U,0x0309U,1U,0x01C0U,1U);
    A_CanUserTest_Inject(1U,0x0309U,1U,0x10000U,1U);
    A_CanUserTest_Pump(&g_context,90U,7U);
    for(i=0U;i<3U;i++) { A_CanUserTest_Expect(6U,0x0309U,A_HOSTCAN_CODE_INTERLOCK,1U); }
    A_CanUserTest_Inject(1U,0x0307U,1U,0U,1U);
    A_HostCan_Process(&g_context,100U);
    assert(A_HostCan_TakeCommand(&g_context,&g_command));
    assert(A_HostCan_CommandActive(&g_context,g_command.sequence,1599U));
    assert(!A_HostCan_CommandActive(&g_context,g_command.sequence,g_command.started_ms+A_HOSTCAN_COMMAND_TIMEOUT_MS));
    assert(!A_HostCan_CompleteCommand(&g_context,g_command.sequence,0U,g_command.started_ms+A_HOSTCAN_COMMAND_TIMEOUT_MS));
    A_CanUserTest_Pump(&g_context,g_command.started_ms+A_HOSTCAN_COMMAND_TIMEOUT_MS,4U);
    A_CanUserTest_Expect(6U,0x0307U,A_HOSTCAN_CODE_EXECUTION_TIMEOUT,1U);
    A_CanUserTest_Inject(2U,0U,1U,0U,1U);
    A_CanUserTest_Pump(&g_context,2011U,4U);
    A_CanUserTest_Empty(); assert(g_context.last_error_code==A_HOSTCAN_CODE_STALE);

    A_CanUserTest_Reset(&g_context);
    H_CanMock_SetAck(0U);
    A_CanUserTest_Inject(2U,0x0100U,1U,0U,1U);
    A_CanUserTest_Pump(&g_context,1U,4U);
    A_CanUserTest_Expect(7U,0x0100U,0U,1U);
    A_HostCan_Process(&g_context,103U);
    assert(g_context.failed_transmissions==1U && g_context.transmit_count==0U);
    H_CanMock_SetAck(1U);
    A_CanUserTest_Inject(2U,0x0100U,1U,0U,1U);
    A_CanUserTest_Pump(&g_context,104U,4U);
    A_CanUserTest_Expect(7U,0x0100U,0U,1U);
    A_CanUserTest_Empty();

    A_CanUserTest_Reset(&g_context);
    g_system.valves_valid=1U; g_system.valve_outputs=0U; g_system.valve_target=0U;
    assert(A_HostCan_PublishSystem(&g_context,&g_system));
    A_HostCan_SetExecutors(&g_context,A_HOSTCAN_EXECUTOR_VALVE | A_HOSTCAN_EXECUTOR_MFC);
    A_CanUserTest_Inject(1U,0x0308U,1U,1U,1U);
    A_HostCan_Process(&g_context,UINT32_MAX-9U);
    assert(A_HostCan_TakeCommand(&g_context,&g_command));
    assert(A_HostCan_CommandActive(&g_context,g_command.sequence,5U)); // 毫秒计数回绕
    A_HostCan_SetExecutors(&g_context,A_HOSTCAN_EXECUTOR_MFC); // 部分撤销也使阀门旧命令失效
    assert(!A_HostCan_CommandActive(&g_context,g_command.sequence,6U));
    assert(!A_HostCan_CompleteCommand(&g_context,g_command.sequence,0U,6U));
    A_CanUserTest_Pump(&g_context,6U,4U);
    A_CanUserTest_Expect(6U,0x0308U,A_HOSTCAN_CODE_NOT_READY,1U);

    A_CanUserTest_Inject(3U,0x0300U,1U,1U,1U); // 未使用的广播不执行
    A_CanUserTest_Inject(1U,0x0300U,1U,1U,127U); // 广播来源不接受
    A_CanUserTest_Inject(1U,0x00FFU,1U,1U,1U); // 类型区合法但参数未定义
    A_CanUserTest_Pump(&g_context,12U,5U);
    assert(g_context.ignored_frames==2U);
    A_CanUserTest_Expect(6U,0x00FFU,A_HOSTCAN_CODE_ADDRESS,1U);
    A_CanUserTest_Empty();

    A_CanUserTest_Reset(&g_context);
    A_CanUserTest_Channels(&g_context);
    H_CanMock_SetAck(0U);
    for(i=0U;i<16U;i++) { A_CanUserTest_Inject(2U,0U,6U,0U,1U); }
    A_CanUserTest_Pump(&g_context,20U,6U);
    assert(g_context.transmit_count<=A_HOSTCAN_TX_CAPACITY);
    assert(g_context.transport.hardware.receive_count>0U); // 拥塞时保留请求，不溢出回复队列
    H_CanMock_Event(CAN_EVENT_TX_COMPLETE);
    H_CanMock_SetAck(1U);
    A_CanUserTest_Pump(&g_context,30U,200U);
    for(i=0U;i<96U;i++) { A_CanUserTest_Expect(7U,(uint16_t)(i%6U),F_CanUser_FloatToBits((float)(i%6U)+1.0F),1U); }
    A_CanUserTest_Empty();
}
/*
 * 说明：运行全部协议及业务测试
 * 输入：无
 * 输出：无
 */
void A_CanUserTest_Run(void)
{
    A_CanUserTest_Codec();
    A_CanUserTest_Hardware();
    A_CanUserTest_Application();
    puts("CAN_USER: wire compatibility, boundaries, ISR queue, reads, deferred writes, interlocks and recovery PASS");
}
/*
 * 说明：本机测试程序入口
 * 输入：无
 * 输出：int 退出码
 */
int main(void)
{
    A_CanUserTest_Run();
    return 0;
}
