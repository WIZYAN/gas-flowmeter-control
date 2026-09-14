#include "HostCanStack.h"
#include "A_HostCan.h"

#if configTICK_RATE_HZ != 1000
#error "HostCanStack time conversion must be updated when the RTOS tick is not 1 ms."
#endif

static A_HostCan_Context g_host_can = {0}; // HostCanStack持有；后续由启动层将指针交给业务任务

/*
 * 说明：上位机CAN任务入口，运行CAN_USER从机
 * 输入：pvParameters FSP任务参数
 * 输出：无
 */
void HostCanStack_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED (pvParameters);

    while (0U == A_HostCan_Initialize(&g_host_can, 1U))
    {
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
    while (1)
    {
        A_HostCan_Process(&g_host_can, (uint32_t) xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}
