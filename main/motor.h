#ifndef _MOTOR_H__
#define _MOTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MT_PLATE_CMD = 0,
    MT_PLATE_STOP_CMD,
    MT_MAIN_CMD,
    MT_WASTE_CMD,
    MT_SCP_INOUT_CMD,
    MT_SCP_SPIN_CMD,
    MT_PAUSE_CMD,
    MT_RESTORE_CMD,
    MT_RESET_CMD,
    MT_CMD_MAX
} mt_cmd_t;

typedef enum {
	MT_IDLE_MODE = 0,
	MAIN_MOTOR_MODE,
	MAIN_MOTOR_MODE_1,
	WASTE_MOTOR_MODE,
	WASTE_MOTOR_MODE_1,
	SCPINOUT_MOTOR_MODE,
	SCPINOUT_MOTOR_MODE_1,
	SCPSPIN_MOTOR_MODE,
	SCPSPIN_MOTOR_MODE_1,
	MT_ERROR_TIMEOUT,
	MT_MODE_MAX
} mt_mode_t;


typedef struct {
    uint32_t task_id;
    uint32_t cmd;
    uint32_t angle;
    uint32_t direction;
    uint32_t timeout;
    uint32_t cal;
} mt_message_t;

typedef enum
{
	WASTE_FORWARD_CMD = 0x01,	// STEP MT
	WASTE_REVERSE_CMD,
    WASTE_STOP_CMD, 
    PLATE_FORWARD_CMD,			// DC MT
    PLATE_REVERSE_CMD,
    PLATE_STOP_CMD, 
    SCP_INOUT_FORWARD_CMD,		// DC MT
    SCP_INOUT_REVERSE_CMD,
    SCP_INOUT_STOP_CMD, 
    SCP_SPIN_FORWARD_CMD,      	// STEP MT
    SCP_SPIN_REVERSE_CMD,
    SCP_SPIN_STOP_CMD, 
    MAIN_FORWARD_CMD,      		// STEP MT
    MAIN_REVERSE_CMD,
    MAIN_STOP_CMD, 
    
} DC_MOTOR_CMD_T;

/* Includes ------------------------------------------------------------------*/

void send_motor_msg(void *message, uint32_t cmd);
int check_motor(void);

void motor_task_init(void);


#ifdef __cplusplus
}
#endif

#endif /* _MOTOR_H__ */
