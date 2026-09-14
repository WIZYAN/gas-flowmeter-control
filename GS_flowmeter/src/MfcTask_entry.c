#include "MfcTask.h"
#include "A_EX201.h"

static A_EX201_Context g_ex201_context = {0}; // MfcTask持有的EX-201S事务上下文

/*
 * 说明：处理流量计侧SPI通信完成及错误事件
 * 输入：p_args FSP SPI回调参数
 * 输出：无
 */
void H_MFC_CAN_SpiCallback(spi_callback_args_t *p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
}

/*
 * 说明：流量计通信任务入口
 * 输入：pvParameters FreeRTOS任务参数
 * 输出：无
 */
void MfcTask_entry(void *pvParameters)
{
    A_EX201_Result initialize_result = A_EX201_RESULT_OK; // EX-201S事务模块初始化结果

    FSP_PARAMETER_NOT_USED (pvParameters);

    initialize_result = A_EX201_Initialize(&g_ex201_context);

    if (A_EX201_RESULT_OK != initialize_result)
    {
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
    }

    while (1)
    {
        A_EX201_Process(&g_ex201_context, xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}
