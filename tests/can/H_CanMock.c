#include "H_CanMock.h"
#include "HostCanStack.h"
#include <assert.h>
#include <string.h>
typedef struct {
 void (*callback)(can_callback_args_t *); // 驱动回调
 const void *context; // 驱动上下文
 can_frame_t frames[1024]; // 捕获的发送帧
 uint32_t count; // 队列长度
 uint32_t index; // 队列读位置
 uint32_t ack; // 是否立即完成
 uint32_t open; // 打开标志
 uint32_t critical; // 临界区嵌套深度
} H_CanMock_State;
static H_CanMock_State g_mock; // 仅测试使用的驱动状态
/*
 * 说明：模拟FSP打开
 * 输入：p_ctrl 控制块，p_cfg 配置
 * 输出：fsp_err_t 结果
 */
static fsp_err_t H_CanMock_Open(void *p_ctrl, const void *p_cfg)
{
 (void)p_ctrl; (void)p_cfg;
 if (g_mock.open) { return -1; }
 g_mock.open=1U; return FSP_SUCCESS;
}
/*
 * 说明：模拟FSP关闭
 * 输入：p_ctrl 控制块
 * 输出：fsp_err_t 结果
 */
static fsp_err_t H_CanMock_Close(void *p_ctrl)
{
 (void)p_ctrl; g_mock.open=0U; return FSP_SUCCESS;
}
/*
 * 说明：模拟回调绑定
 * 输入：p_ctrl 控制块，callback 回调，context 上下文，memory 回调缓冲
 * 输出：fsp_err_t 结果
 */
static fsp_err_t H_CanMock_CallbackSet(void *p_ctrl, void (*callback)(can_callback_args_t *),
                                     const void *context, can_callback_args_t *memory)
{
 (void)p_ctrl; (void)memory; g_mock.callback=callback; g_mock.context=context; return FSP_SUCCESS;
}
/*
 * 说明：注入驱动事件
 * 输入：event 中断事件
 * 输出：无
 */
void H_CanMock_Event(can_event_t event)
{
 can_callback_args_t g_args={0}; // 回调参数
 assert(g_mock.callback);
 g_args.event=event; g_args.p_context=g_mock.context;
 g_mock.callback(&g_args);
}
/*
 * 说明：捕获发送并模拟立即发送完成中断
 * 输入：p_ctrl 控制块，mailbox 邮箱，p_frame 帧
 * 输出：fsp_err_t 结果
 */
static fsp_err_t H_CanMock_Write(void *p_ctrl, uint32_t mailbox, can_frame_t *p_frame)
{
 (void)p_ctrl; assert(mailbox==0U); assert(g_mock.open); assert(g_mock.count<1024U);
 assert(p_frame->id_mode==CAN_ID_MODE_EXTENDED && p_frame->type==CAN_FRAME_TYPE_DATA);
 assert(p_frame->data_length_code==8U);
 g_mock.frames[g_mock.count++]=*p_frame;
 if(g_mock.ack) { H_CanMock_Event(CAN_EVENT_TX_COMPLETE); }
 return FSP_SUCCESS;
}
static const H_CanMock_Api g_mock_api={H_CanMock_Open,H_CanMock_Close,H_CanMock_CallbackSet,H_CanMock_Write}; // FSP接口表
const H_CanMock_Instance g_can0={&g_mock_api,NULL,NULL}; // 替代生成的外设实例
/*
 * 说明：重置测试驱动
 * 输入：无
 * 输出：无
 */
void H_CanMock_Reset(void)
{
 assert(g_mock.critical==0U); memset(&g_mock,0,sizeof(g_mock)); g_mock.ack=1U;
}
/*
 * 说明：注入接收事件
 * 输入：p_frame 输入帧
 * 输出：无
 */
void H_CanMock_Inject(const can_frame_t *p_frame)
{
 can_callback_args_t g_args={0}; // 接收回调
 assert(g_mock.callback); g_args.event=CAN_EVENT_RX_COMPLETE;
 g_args.p_context=g_mock.context; g_args.frame=*p_frame; g_args.p_frame=&g_args.frame;
 g_mock.callback(&g_args);
}
/*
 * 说明：提取捕获帧
 * 输入：p_frame 输出帧
 * 输出：uint32_t 是否有帧
 */
uint32_t H_CanMock_Pop(can_frame_t *p_frame)
{
 if(g_mock.index==g_mock.count) { return 0U; }
 *p_frame=g_mock.frames[g_mock.index++]; return 1U;
}
/*
 * 说明：设置模拟ACK
 * 输入：enabled 非0启用
 * 输出：无
 */
void H_CanMock_SetAck(uint32_t enabled) { g_mock.ack=enabled; }
/*
 * 说明：模拟临界区进入
 * 输入：无
 * 输出：无
 */
void H_CanMock_EnterCritical(void) { g_mock.critical++; }
/*
 * 说明：模拟临界区退出并检查配对
 * 输入：无
 * 输出：无
 */
void H_CanMock_ExitCritical(void) { assert(g_mock.critical>0U); g_mock.critical--; }

