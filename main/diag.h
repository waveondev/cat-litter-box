#ifndef __DIAG_H__
#define __DIAG_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DIAG_IDLE_CMD = 0,
    DIAG_NEXT_STEP_CMD,
    DIAG_NEXT_FUNC_CMD,
    DIAG_CMD_MAX
} diag_cmd_t;

typedef enum {
    DIAG_IDLE_MODE,
    DIAG_SENSOR_RF_MODE,		// MAIN COVER OPEN
    DIAG_PLATE_MODE,
    DIAG_SCPINOUT_MODE,
    DIAG_SCPSPIN_MODE,
    DIAG_WASTE_MODE,
    DIAG_FINISH_MODE,			// MAIN COVER CLOSE, AND REBOOT
    DIAG_MODE_MAX
} diag_mode_t;


/* Includes ------------------------------------------------------------------*/

bool get_status_diag(void);
void set_status_diag(bool enable);
void send_diag_cmd_msg(void *message, uint32_t cmd);
void diag_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __DIAG_H__ */
