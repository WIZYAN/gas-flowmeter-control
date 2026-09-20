/*
 * Created on: 2026年9月19日
 * Author: CI
 */
#include "A_Valve.h"

/*
 * 说明：通过功能层初始化输出
 * 输入：p_context ControlTask独占状态
 * 输出：F_Valve_Result 结果
 */
F_Valve_Result A_Valve_Initialize(A_Valve_Context *p_context)
{
    return F_Valve_Initialize(p_context);
}

/*
 * 说明：V1～V6独立，V7/V9必须相同且不能与V8同时开
 * 输入：p_context 状态，target 已由CAN形成的完整目标，now_ms 当前毫秒时间
 * 输出：F_Valve_Result 结果，联锁失败时原输出保持不变
 */
F_Valve_Result A_Valve_SetTarget(A_Valve_Context *p_context, uint32_t target, uint32_t now_ms)
{
    uint32_t group79 = target & A_VALVE_GROUP79; // V7/V9目标
    if ((target & ~F_VALVE_OUTPUT_MASK) != 0U ||
        (group79 != 0U && group79 != A_VALVE_GROUP79) ||
        (group79 != 0U && (target & A_VALVE8) != 0U))
    {
        return F_VALVE_INVALID_TARGET;
    }
    return F_Valve_SetTarget(p_context, target, now_ms);
}

/*
 * 说明：通过功能层推进九阀计时
 * 输入：p_context 状态，now_ms 当前毫秒时间
 * 输出：F_Valve_Result 结果
 */
F_Valve_Result A_Valve_Process(A_Valve_Context *p_context, uint32_t now_ms)
{
    return F_Valve_Process(p_context, now_ms);
}

/*
 * 说明：通过功能层撤销仍在吸合的输出
 * 输入：p_context 状态，now_ms 当前毫秒时间
 * 输出：F_Valve_Result 结果
 */
F_Valve_Result A_Valve_Cancel(A_Valve_Context *p_context, uint32_t now_ms)
{
    return F_Valve_Cancel(p_context, now_ms);
}
