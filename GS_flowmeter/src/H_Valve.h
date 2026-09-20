/*
 * Created on: 2026年9月19日
 * Author: CI
 */
#ifndef SRC_H_VALVE_H_
#define SRC_H_VALVE_H_
#include <stdint.h>

/*
 * 说明：将九路阀开关和三路升压控制配置为GPIO低输出，只在启动阶段调用
 * 输入：无，FSP的IOPORT必须已经打开
 * 输出：uint32_t 非0成功；返回值仅表示GPIO操作结果，不代表实际阀位
 */
uint32_t H_Valve_Initialize(void);
/*
 * 说明：写九路开关电平，V7和V9通过同一次端口写操作同步更新
 * 输入：outputs bit0～bit8对应V1～V9，1为线圈通电
 * 输出：uint32_t 非0成功
 */
uint32_t H_Valve_SetOutputs(uint32_t outputs);
/*
 * 说明：控制三组12V开关；置0后由硬件二极管通路提供约5V
 * 输入：boost_mask bit0～bit2对应VALP1～VALP3，1为接通12V
 * 输出：uint32_t 非0成功，实装MOS型号和输出电压仍待实板验证
 */
uint32_t H_Valve_SetBoost(uint32_t boost_mask);
#endif
