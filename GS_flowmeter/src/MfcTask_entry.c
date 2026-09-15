#include "MfcTask.h"
#include "A_System.h"

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
    A_System_Context *p_system = A_System_GetContext(); // MfcTask持有其中的六路轮询状态
    A_MFC_Config g_config = {0}; // 六个通道地址配置

    FSP_PARAMETER_NOT_USED (pvParameters);

    A_MFC_DefaultConfig(&g_config);
    if (0U == A_MFC_Initialize(p_system->p_mfc, &g_config, xTaskGetTickCount()))
    {
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
    }

    while (1)
    {
        A_System_ProcessMfc(p_system, xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}
