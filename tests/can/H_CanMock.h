#ifndef TEST_H_CAN_MOCK_H
#define TEST_H_CAN_MOCK_H
#include "r_can_api.h"
/*
 * 说明：清理FSP测试替身
 * 输入：无
 * 输出：无
 */
void H_CanMock_Reset(void);
/*
 * 说明：注入一帧接收中断
 * 输入：p_frame 待注入帧
 * 输出：无
 */
void H_CanMock_Inject(const can_frame_t *p_frame);
/*
 * 说明：弹出已交给CAN硬件的测试帧
 * 输入：p_frame 输出帧
 * 输出：uint32_t 非0取得帧
 */
uint32_t H_CanMock_Pop(can_frame_t *p_frame);
/*
 * 说明：设置发送是否自动完成
 * 输入：enabled 非0模拟ACK，0模拟无ACK
 * 输出：无
 */
void H_CanMock_SetAck(uint32_t enabled);
/*
 * 说明：注入CAN错误事件
 * 输入：event 事件
 * 输出：无
 */
void H_CanMock_Event(can_event_t event);
/*
 * 说明：检查测试临界区进入
 * 输入：无
 * 输出：无
 */
void H_CanMock_EnterCritical(void);
/*
 * 说明：检查测试临界区退出
 * 输入：无
 * 输出：无
 */
void H_CanMock_ExitCritical(void);
#endif

