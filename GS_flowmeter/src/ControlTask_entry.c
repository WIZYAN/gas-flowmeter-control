#include "ControlTask.h"
#include "A_System.h"

/*
 * 说明：每1ms推进九阀吸合/保持，并通过静态队列协调CAN请求及MFC结果
 * 输入：pvParameters FSP任务参数
 * 输出：无
 */
void ControlTask_entry(void *pvParameters)
{
    A_System_Context *p_system = A_System_GetContext(); // 板级长期有效上下文
    FSP_PARAMETER_NOT_USED (pvParameters);

    while (1)
    {
        A_System_ProcessControl(p_system, xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}
