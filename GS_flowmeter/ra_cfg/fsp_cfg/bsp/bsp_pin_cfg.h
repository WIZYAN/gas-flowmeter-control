/* generated configuration header file - do not edit */
#ifndef BSP_PIN_CFG_H_
#define BSP_PIN_CFG_H_
#include "r_ioport.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

#define VALP2 (BSP_IO_PORT_00_PIN_00) /* VALP2 */
#define VAL2 (BSP_IO_PORT_00_PIN_01) /* VAL2 */
#define VAL3 (BSP_IO_PORT_00_PIN_02) /* VAL3 */
#define VAL1 (BSP_IO_PORT_00_PIN_03) /* VAL1 */
#define VALP1 (BSP_IO_PORT_00_PIN_04) /* VALP1 */
#define CAN_RXD0 (BSP_IO_PORT_01_PIN_02) /* CAN_RXD0 */
#define CAN_TXD0 (BSP_IO_PORT_01_PIN_03) /* CAN_TXD0 */
#define SWDIO (BSP_IO_PORT_01_PIN_08) /* SWDIO */
#define SPI1_MOSI (BSP_IO_PORT_01_PIN_09) /* SPI1_MOSI */
#define SPI1_MISO (BSP_IO_PORT_01_PIN_10) /* SPI1_MISO */
#define SPI1_SCK (BSP_IO_PORT_01_PIN_11) /* SPI1_SCK */
#define SPI1_NSS (BSP_IO_PORT_01_PIN_12) /* SPI1_NSS */
#define MFC_LINKOUT (BSP_IO_PORT_02_PIN_05) /* MFC_LINKOUT */
#define MFC_LINKIN (BSP_IO_PORT_02_PIN_06) /* MFC_LINKIN */
#define SWCLK (BSP_IO_PORT_03_PIN_00) /* SWCLK */
#define MFC_RXD2 (BSP_IO_PORT_03_PIN_01) /* MFC_RXD2 */
#define MFC_TXD2 (BSP_IO_PORT_03_PIN_02) /* MFC_TXD2 */
#define MFC_RES2 (BSP_IO_PORT_03_PIN_03) /* MFC_RES2 */
#define MFC_EN2 (BSP_IO_PORT_03_PIN_04) /* MFC_EN2 */
#define VAL4 (BSP_IO_PORT_04_PIN_00) /* VAL4 */
#define VAL6 (BSP_IO_PORT_04_PIN_01) /* VAL6 */
#define VAL5 (BSP_IO_PORT_04_PIN_02) /* VAL5 */
#define STATUS (BSP_IO_PORT_04_PIN_07) /* STATUS */
#define VAL9 (BSP_IO_PORT_04_PIN_08) /* VAL9 */
#define VALP3 (BSP_IO_PORT_04_PIN_09) /* VALP3 */
#define VAL7 (BSP_IO_PORT_04_PIN_10) /* VAL7 */
#define VAL8 (BSP_IO_PORT_04_PIN_11) /* VAL8 */
#define RS485_RES0 (BSP_IO_PORT_05_PIN_01) /* RS485_RES0 */
#define RS485_EN0 (BSP_IO_PORT_05_PIN_02) /* RS485_EN0 */
extern const ioport_cfg_t g_bsp_pin_cfg; /* R7FA4M1AB3CFM.pincfg */

void BSP_PinConfigSecurityInit();

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif /* BSP_PIN_CFG_H_ */
