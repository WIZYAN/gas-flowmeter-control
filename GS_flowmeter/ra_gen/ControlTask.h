/* generated thread header file - do not edit */
#ifndef CONTROLTASK_H_
#define CONTROLTASK_H_
#include "bsp_api.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "hal_data.h"
#ifdef __cplusplus
                extern "C" void ControlTask_entry(void * pvParameters);
                #else
extern void ControlTask_entry(void *pvParameters);
#endif
FSP_HEADER
FSP_FOOTER
#endif /* CONTROLTASK_H_ */
