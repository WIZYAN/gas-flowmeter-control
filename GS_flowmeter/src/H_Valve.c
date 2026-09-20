/*
 * Created on: 2026年9月19日
 * Author: CI
 */
#include "H_Valve.h"
#include "hal_data.h"

/*
 * 说明：九路逻辑位转换为实际端口位，仅修改阀门引脚，不影响同端口其他外设
 * 输入：outputs 九阀线圈通电掩码
 * 输出：uint32_t 非0表示两个端口均写成功
 */
uint32_t H_Valve_SetOutputs(uint32_t outputs)
{
    uint16_t port0 = 0U; // P003=V1、P001=V2、P002=V3
    uint16_t port4 = 0U; // P400=V4、P402=V5、P401=V6、P410=V7、P411=V8、P408=V9
    fsp_err_t result0 = FSP_SUCCESS; // PORT0执行结果
    fsp_err_t result4 = FSP_SUCCESS; // PORT4执行结果
    if ((outputs & ~0x01FFUL) != 0U)
    {
        return 0U;
    }
    port0 = (uint16_t) (((outputs & 0x01U) << 3U) | (outputs & 0x06U));
    port4 = (uint16_t) (((outputs & 0x08U) >> 3U) | ((outputs & 0x10U) >> 2U) |
        ((outputs & 0x20U) >> 4U) | ((outputs & 0xC0U) << 4U) | (outputs & 0x100U));
    result4 = R_IOPORT_PortWrite(&g_ioport_ctrl, BSP_IO_PORT_04, port4, 0x0D07U);
    result0 = R_IOPORT_PortWrite(&g_ioport_ctrl, BSP_IO_PORT_00, port0, 0x000EU);
    return (result0 == FSP_SUCCESS && result4 == FSP_SUCCESS) ? 1U : 0U;
}

/*
 * 说明：按原理图高有效控制VALP1/P004、VALP2/P000、VALP3/P409
 * 输入：boost_mask 三组12V控制位
 * 输出：uint32_t 非0成功
 */
uint32_t H_Valve_SetBoost(uint32_t boost_mask)
{
    uint16_t port0 = 0U; // 第一、二组升压引脚
    uint16_t port4 = 0U; // 第三组升压引脚
    fsp_err_t result0 = FSP_SUCCESS; // PORT0执行结果
    fsp_err_t result4 = FSP_SUCCESS; // PORT4执行结果
    if ((boost_mask & ~0x07UL) != 0U)
    {
        return 0U;
    }
    port0 = (uint16_t) (((boost_mask & 1U) << 4U) | ((boost_mask & 2U) >> 1U));
    port4 = (uint16_t) ((boost_mask & 4U) << 7U);
    result0 = R_IOPORT_PortWrite(&g_ioport_ctrl, BSP_IO_PORT_00, port0, 0x0011U);
    result4 = R_IOPORT_PortWrite(&g_ioport_ctrl, BSP_IO_PORT_04, port4, 0x0200U);
    return (result0 == FSP_SUCCESS && result4 == FSP_SUCCESS) ? 1U : 0U;
}

/*
 * 说明：启动时逐脚确认为GPIO低输出，失败也继续尝试关闭其余输出
 * 输入：无
 * 输出：uint32_t 非0成功
 */
uint32_t H_Valve_Initialize(void)
{
    static const bsp_io_port_pin_t s_pins[12] = {
        VAL1, VAL2, VAL3, VAL4, VAL5, VAL6, VAL7, VAL8, VAL9, VALP1, VALP2, VALP3
    }; // 与configuration.xml的符号名一致
    uint32_t index = 0U; // 当前引脚
    uint32_t success = 1U; // 累计结果，禁止被后续成功覆盖
    for (index = 0U; index < 12U; index++)
    {
        if (R_IOPORT_PinCfg(&g_ioport_ctrl, s_pins[index],
            IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW) != FSP_SUCCESS)
        {
            success = 0U;
        }
    }
    if (!H_Valve_SetOutputs(0U))
    {
        success = 0U;
    }
    if (!H_Valve_SetBoost(0U))
    {
        success = 0U;
    }
    return success;
}
