/* Created on: 2026年9月20日，Author: CI */
#ifndef MFC_A_MFC_LINK_H_
#define MFC_A_MFC_LINK_H_
#include "A_MFC.h"
/*
 * 说明：只读探测、连续两次确认及整链路失效重探测，不执行流量写入
 * 输入：p_context MfcTask状态，now 当前节拍；没有在途写事务时调用
 * 输出：uint32_t 非0表示后端锁定且可供六路调度使用
 */
uint32_t A_MfcLink_Process(A_MFC_Context *p_context, TickType_t now);
#endif
