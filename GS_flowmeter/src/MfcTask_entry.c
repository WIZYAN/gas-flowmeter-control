#include "MfcTask.h"

void mfc_uart_callback(uart_callback_args_t *p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
}

void spi_callback(spi_callback_args_t *p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
}

/* MfcTask entry function */
/* pvParameters contains TaskHandle_t */
void MfcTask_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED (pvParameters);

    /* TODO: add your own code here */
    while (1)
    {
        vTaskDelay (1);
    }
}
