#ifndef TEST_HOST_CAN_STACK_H
#define TEST_HOST_CAN_STACK_H
#include <stddef.h>
#include "r_can_api.h"
#include "H_CanMock.h"
#define FSP_CRITICAL_SECTION_DEFINE
#define FSP_CRITICAL_SECTION_ENTER H_CanMock_EnterCritical()
#define FSP_CRITICAL_SECTION_EXIT H_CanMock_ExitCritical()
typedef struct {
 fsp_err_t (*open)(void *, const void *);
 fsp_err_t (*close)(void *);
 fsp_err_t (*callbackSet)(void *, void (*)(can_callback_args_t *), const void *, can_callback_args_t *);
 fsp_err_t (*write)(void *, uint32_t, can_frame_t *);
} H_CanMock_Api;
typedef struct { const H_CanMock_Api *p_api; void *p_ctrl; const void *p_cfg; } H_CanMock_Instance;
extern const H_CanMock_Instance g_can0;
#endif

