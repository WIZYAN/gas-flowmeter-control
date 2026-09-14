#ifndef TEST_R_CAN_API_H
#define TEST_R_CAN_API_H
#include <stdint.h>
typedef int fsp_err_t;
#define FSP_SUCCESS 0
typedef enum { CAN_ID_MODE_STANDARD, CAN_ID_MODE_EXTENDED } can_id_mode_t;
typedef enum { CAN_FRAME_TYPE_DATA, CAN_FRAME_TYPE_REMOTE } can_frame_type_t;
typedef enum {
 CAN_EVENT_ERR_WARNING=2, CAN_EVENT_ERR_PASSIVE=4, CAN_EVENT_ERR_BUS_OFF=8,
 CAN_EVENT_BUS_RECOVERY=16, CAN_EVENT_MAILBOX_MESSAGE_LOST=32,
 CAN_EVENT_ERR_BUS_LOCK=128, CAN_EVENT_TX_ABORTED=512,
 CAN_EVENT_RX_COMPLETE=1024, CAN_EVENT_TX_COMPLETE=2048
} can_event_t;
typedef struct { uint32_t id; can_id_mode_t id_mode; can_frame_type_t type;
 uint8_t data_length_code; uint32_t options; uint8_t data[8]; } can_frame_t;
typedef struct { uint32_t channel; can_event_t event; uint32_t error;
 uint32_t mailbox; can_frame_t *p_frame; void const *p_context; can_frame_t frame; } can_callback_args_t;
#endif

