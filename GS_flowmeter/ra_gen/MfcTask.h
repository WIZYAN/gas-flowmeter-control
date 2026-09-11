/* generated thread header file - do not edit */
#ifndef MFCTASK_H_
#define MFCTASK_H_
#include "bsp_api.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "hal_data.h"
#ifdef __cplusplus
                extern "C" void MfcTask_entry(void * pvParameters);
                #else
extern void MfcTask_entry(void *pvParameters);
#endif
#include "r_dtc.h"
#include "r_transfer_api.h"
#include "r_spi.h"
#include "r_sci_uart.h"
#include "r_uart_api.h"
FSP_HEADER
/* Transfer on DTC Instance. */
extern const transfer_instance_t g_transfer1;

/** Access the DTC instance using these structures when calling API functions directly (::p_api is not used). */
extern dtc_instance_ctrl_t g_transfer1_ctrl;
extern const transfer_cfg_t g_transfer1_cfg;
/* Transfer on DTC Instance. */
extern const transfer_instance_t g_transfer0;

/** Access the DTC instance using these structures when calling API functions directly (::p_api is not used). */
extern dtc_instance_ctrl_t g_transfer0_ctrl;
extern const transfer_cfg_t g_transfer0_cfg;
/** SPI on SPI Instance. */
extern const spi_instance_t g_mfc_spi;

/** Access the SPI instance using these structures when calling API functions directly (::p_api is not used). */
extern spi_instance_ctrl_t g_mfc_spi_ctrl;
extern const spi_cfg_t g_mfc_spi_cfg;

/** Callback used by SPI Instance. */
#ifndef spi_callback
void spi_callback(spi_callback_args_t *p_args);
#endif

#define RA_NOT_DEFINED (1)
#if (RA_NOT_DEFINED == g_transfer0)
    #define g_mfc_spi_P_TRANSFER_TX (NULL)
#else
#define g_mfc_spi_P_TRANSFER_TX (&g_transfer0)
#endif
#if (RA_NOT_DEFINED == g_transfer1)
    #define g_mfc_spi_P_TRANSFER_RX (NULL)
#else
#define g_mfc_spi_P_TRANSFER_RX (&g_transfer1)
#endif
#undef RA_NOT_DEFINED
/** UART on SCI Instance. */
extern const uart_instance_t g_mfc_uart;

/** Access the UART instance using these structures when calling API functions directly (::p_api is not used). */
extern sci_uart_instance_ctrl_t g_mfc_uart_ctrl;
extern const uart_cfg_t g_mfc_uart_cfg;
extern const sci_uart_extended_cfg_t g_mfc_uart_cfg_extend;

#ifndef mfc_uart_callback
void mfc_uart_callback(uart_callback_args_t *p_args);
#endif
FSP_FOOTER
#endif /* MFCTASK_H_ */
