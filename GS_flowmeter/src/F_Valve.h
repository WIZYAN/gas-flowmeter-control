/*
 * Created on: 2026年9月19日
 * Author: CI
 */
#ifndef SRC_F_VALVE_H_
#define SRC_F_VALVE_H_
#include <stdint.h>

#define F_VALVE_COUNT (9U) // 九路电磁阀
#define F_VALVE_PULL_IN_MS (200U) // 新开阀的12V吸合窗口，单位ms
#define F_VALVE_OUTPUT_MASK (0x01FFUL) // 九阀有效位

typedef enum
{
    F_VALVE_OK = 0, // GPIO执行成功
    F_VALVE_NOT_READY, // 未初始化
    F_VALVE_INVALID_TARGET, // 非法目标
    F_VALVE_DRIVER_ERROR // GPIO失败，停止继续开阀
} F_Valve_Result;

typedef struct
{
    uint32_t outputs; // 最近成功施加的线圈输出，故障时不能作为真实状态使用
    uint32_t target; // 已接受的九阀目标
    uint32_t pull_in_mask; // 仍在200ms吸合窗口内的阀
    uint32_t boost_mask; // 已施加的三组12V控制位
    uint32_t started_ms[F_VALVE_COUNT]; // 每个新开阀的独立起点，重复开命令不刷新
    uint32_t initialized; // 已执行过初始化，故障后不自动重试开阀
    uint32_t fault; // GPIO错误锁存，重新启动后才能重新初始化
} F_Valve_Context;

/*
 * 说明：初始化九阀为关闭，禁止重复初始化时打断正在运行的输出
 * 输入：p_context 零初始化的本任务独占状态
 * 输出：F_Valve_Result 结果
 */
F_Valve_Result F_Valve_Initialize(F_Valve_Context *p_context);
/*
 * 说明：先关闭撤销的输出，再接通新开阀所需12V，最后打开新输出
 * 输入：p_context 状态，target 已通过应用层联锁的掩码，now_ms 当前毫秒时间
 * 输出：F_Valve_Result 结果；成功仍须周期Process推进吸合到保持
 */
F_Valve_Result F_Valve_SetTarget(F_Valve_Context *p_context, uint32_t target, uint32_t now_ms);
/*
 * 说明：推进200ms计时，最后一个吸合窗口结束后将所在组转为约5V保持
 * 输入：p_context 状态，now_ms 当前毫秒时间，允许32位回绕
 * 输出：F_Valve_Result 结果，无阻塞延时
 */
F_Valve_Result F_Valve_Process(F_Valve_Context *p_context, uint32_t now_ms);
/*
 * 说明：未完成请求失效时关闭仍在吸合的阀，保留此前已保持的阀，不恢复已关闭互斥侧
 * 输入：p_context 状态，now_ms 当前毫秒时间
 * 输出：F_Valve_Result 结果
 */
F_Valve_Result F_Valve_Cancel(F_Valve_Context *p_context, uint32_t now_ms);
#endif
