/*
 * Created on: 2026年9月19日
 * Author: CI
 */
#ifndef SRC_A_VALVE_H_
#define SRC_A_VALVE_H_
#include "F_Valve.h"

typedef F_Valve_Context A_Valve_Context; // 同一任务的一份平铺状态，不再套一层结构体
#define A_VALVE_GROUP79 (0x0140UL) // V7和V9必须同开同关
#define A_VALVE8 (0x0080UL) // 与V7/V9互斥

/*
 * 说明：初始化ControlTask独占的九阀模块，全部关闭
 * 输入：p_context 零初始化状态
 * 输出：F_Valve_Result 结果
 */
F_Valve_Result A_Valve_Initialize(A_Valve_Context *p_context);
/*
 * 说明：再次检查九阀联锁后执行目标，非法目标不能产生GPIO操作
 * 输入：p_context 状态，target 九阀目标，now_ms 当前毫秒时间
 * 输出：F_Valve_Result 结果
 */
F_Valve_Result A_Valve_SetTarget(A_Valve_Context *p_context, uint32_t target, uint32_t now_ms);
/*
 * 说明：在ControlTask每轮推进吸合到保持，无额外任务、软件定时器或阻塞等待
 * 输入：p_context 状态，now_ms 当前毫秒时间
 * 输出：F_Valve_Result 结果
 */
F_Valve_Result A_Valve_Process(A_Valve_Context *p_context, uint32_t now_ms);
/*
 * 说明：原CAN请求失效时撤销未完成吸合的输出
 * 输入：p_context 状态，now_ms 当前毫秒时间
 * 输出：F_Valve_Result 结果
 */
F_Valve_Result A_Valve_Cancel(A_Valve_Context *p_context, uint32_t now_ms);
#endif
