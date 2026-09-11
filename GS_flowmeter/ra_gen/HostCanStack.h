/* generated thread header file - do not edit */
#ifndef HOSTCANSTACK_H_
#define HOSTCANSTACK_H_
#include "bsp_api.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "hal_data.h"
#ifdef __cplusplus
                extern "C" void HostCanStack_entry(void * pvParameters);
                #else
extern void HostCanStack_entry(void *pvParameters);
#endif
#include "r_can.h"
#include "r_can_api.h"
FSP_HEADER
/** CAN on CAN Instance. */
extern const can_instance_t g_can0;
/** Access the CAN instance using these structures when calling API functions directly (::p_api is not used). */
extern can_instance_ctrl_t g_can0_ctrl;
extern const can_cfg_t g_can0_cfg;
extern const can_extended_cfg_t g_can0_cfg_extend;

#ifndef can_callback
void can_callback(can_callback_args_t *p_args);
#endif
#define CAN_NO_OF_MAILBOXES_g_can0 (32)
FSP_FOOTER
#endif /* HOSTCANSTACK_H_ */
