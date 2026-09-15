#include "HostCanStack.h"
#include "A_System.h"

#if configTICK_RATE_HZ != 1000
#error "HostCanStack time conversion must be updated when the RTOS tick is not 1 ms."
#endif

/*
 * 说明：上位机CAN任务入口，运行CAN_USER从机
 * 输入：pvParameters FSP任务参数
 * 输出：无
 */
void HostCanStack_entry(void *pvParameters)
{
    A_System_Context *p_system = A_System_GetContext(); // 队列句柄与本任务上下文入口，不读取其他任务业务数据
    FSP_PARAMETER_NOT_USED (pvParameters);

    while (0U == A_HostCan_Initialize(&p_system->host_can, 1U))
    {
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
    while (1)
    {
        A_System_ProcessHostCan(p_system, xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}
