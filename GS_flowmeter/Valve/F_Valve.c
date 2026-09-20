/*
 * Created on: 2026年9月19日
 * Author: CI
 */
#include "F_Valve.h"
#include "H_Valve.h"
#include <stddef.h>

/*
 * 说明：按每三个阀共用一路电源，将吸合位转换为三组12V控制位
 * 输入：pull_in_mask 九阀吸合位
 * 输出：uint32_t 三组控制位
 */
static uint32_t F_Valve_GetBoost(uint32_t pull_in_mask)
{
    uint32_t boost = 0U; // 所需12V电源组
    uint32_t group = 0U; // 电源组索引0～2
    for (group = 0U; group < 3U; group++)
    {
        if ((pull_in_mask & (0x07UL << (group * 3U))) != 0U)
        {
            boost |= 1UL << group;
        }
    }
    return boost;
}

/*
 * 说明：GPIO失败后尽力关闭全部输出和12V，锁存故障，不伪造硬件关闭成功
 * 输入：p_context 状态
 * 输出：F_Valve_Result 固定驱动错误
 */
static F_Valve_Result F_Valve_StopOnError(F_Valve_Context *p_context)
{
    if (H_Valve_SetOutputs(0U))
    {
        p_context->outputs = 0U;
    }
    if (H_Valve_SetBoost(0U))
    {
        p_context->boost_mask = 0U;
    }
    p_context->target = 0U;
    p_context->pull_in_mask = 0U;
    p_context->fault = 1U;
    return F_VALVE_DRIVER_ERROR;
}

/*
 * 说明：第一次调用关闭全部阀，之后只返回已保存的初始化状态
 * 输入：p_context 本任务独占状态
 * 输出：F_Valve_Result 结果
 */
F_Valve_Result F_Valve_Initialize(F_Valve_Context *p_context)
{
    if (p_context == NULL)
    {
        return F_VALVE_NOT_READY;
    }
    if (p_context->initialized != 0U)
    {
        return p_context->fault ? F_VALVE_DRIVER_ERROR : F_VALVE_OK;
    }
    p_context->initialized = 1U;
    if (!H_Valve_Initialize())
    {
        return F_Valve_StopOnError(p_context);
    }
    p_context->outputs = 0U;
    p_context->target = 0U;
    p_context->pull_in_mask = 0U;
    p_context->boost_mask = 0U;
    return F_VALVE_OK;
}

/*
 * 说明：更新九阀目标；电压共享，但每个新开阀的吸合起点独立
 * 输入：p_context 状态，target 目标掩码，now_ms 当前毫秒时间
 * 输出：F_Valve_Result 结果
 */
F_Valve_Result F_Valve_SetTarget(F_Valve_Context *p_context, uint32_t target, uint32_t now_ms)
{
    uint32_t retained = 0U; // 本次仍应通电的旧输出
    uint32_t opened = 0U; // 本次从关变开的输出
    uint32_t boost = 0U; // 本次所需12V组
    uint32_t index = 0U; // 阀索引
    uint32_t bit = 0U; // 当前阀位
    if (p_context == NULL || p_context->initialized == 0U)
    {
        return F_VALVE_NOT_READY;
    }
    if (p_context->fault != 0U)
    {
        return F_VALVE_DRIVER_ERROR;
    }
    if ((target & ~F_VALVE_OUTPUT_MASK) != 0U)
    {
        return F_VALVE_INVALID_TARGET;
    }
    retained = p_context->outputs & target;
    opened = target & ~p_context->outputs;

    // 第1步：关闭不再需要的线圈。互斥侧必须先失电，才允许打开另一侧。
    if (retained != p_context->outputs)
    {
        if (!H_Valve_SetOutputs(retained))
        {
            return F_Valve_StopOnError(p_context);
        }
        p_context->outputs = retained;
    }
    p_context->pull_in_mask &= target;
    for (index = 0U; index < F_VALVE_COUNT; index++)
    {
        bit = 1UL << index;
        if ((opened & bit) != 0U)
        {
            p_context->started_ms[index] = now_ms;
            p_context->pull_in_mask |= bit;
        }
    }

    // 第2步：先准备12V；同组已经保持的阀也会短时升压，用户已允许此行为。
    boost = F_Valve_GetBoost(p_context->pull_in_mask);
    if (boost != p_context->boost_mask)
    {
        if (!H_Valve_SetBoost(boost))
        {
            return F_Valve_StopOnError(p_context);
        }
        p_context->boost_mask = boost;
    }

    // 第3步：接通新线圈。重复打开已开阀不会刷新起点，也不会重新吸合。
    if (target != p_context->outputs)
    {
        if (!H_Valve_SetOutputs(target))
        {
            return F_Valve_StopOnError(p_context);
        }
        p_context->outputs = target;
    }
    p_context->target = target;
    return F_VALVE_OK;
}

/*
 * 说明：只检查时间差，不等待200ms；任务每轮都必须调用，包括结果队列满时
 * 输入：p_context 状态，now_ms 当前毫秒时间
 * 输出：F_Valve_Result 结果
 */
F_Valve_Result F_Valve_Process(F_Valve_Context *p_context, uint32_t now_ms)
{
    uint32_t index = 0U; // 阀索引
    uint32_t bit = 0U; // 当前阀位
    uint32_t boost = 0U; // 尚需保持12V的电源组
    if (p_context == NULL || p_context->initialized == 0U)
    {
        return F_VALVE_NOT_READY;
    }
    if (p_context->fault != 0U)
    {
        return F_VALVE_DRIVER_ERROR;
    }
    for (index = 0U; index < F_VALVE_COUNT; index++)
    {
        bit = 1UL << index;
        if ((p_context->pull_in_mask & bit) != 0U &&
            (uint32_t) (now_ms - p_context->started_ms[index]) >= F_VALVE_PULL_IN_MS)
        {
            p_context->pull_in_mask &= ~bit;
        }
    }
    boost = F_Valve_GetBoost(p_context->pull_in_mask);
    if (boost != p_context->boost_mask)
    {
        if (!H_Valve_SetBoost(boost))
        {
            return F_Valve_StopOnError(p_context);
        }
        p_context->boost_mask = boost;
    }
    return F_VALVE_OK;
}

/*
 * 说明：只撤销尚未完成吸合的输出，取消不能意外重新打开旧通路
 * 输入：p_context 状态，now_ms 当前毫秒时间
 * 输出：F_Valve_Result 结果
 */
F_Valve_Result F_Valve_Cancel(F_Valve_Context *p_context, uint32_t now_ms)
{
    if (p_context == NULL)
    {
        return F_VALVE_NOT_READY;
    }
    return F_Valve_SetTarget(p_context, p_context->outputs & ~p_context->pull_in_mask, now_ms);
}
