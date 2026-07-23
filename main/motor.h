#ifndef _MOTOR_H__
#define _MOTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t task_id;
    uint32_t cmd;
    uint32_t angle;
    uint32_t direction;
    uint32_t timeout;
    bool cal;
} mt_message_t;

typedef enum {
	DC_MOTOR = 0,
	STEP_MOTOR,
	
	MOTOR_MAX
} motor_t;

typedef enum
{
	WASTE_COVER_CMD = 0x01,
    PLATE_CMD,
    SCP_INOUT_CMD,
    SCP_SPIN_CMD,
    MAIN_COVER_CMD,
    
    MOTOR_CMD_MAX
} MOTOR_CMD_T;

typedef enum
{
	FORWARD= 0,
	REVERSE,
    STOP    
} MOTOR_DIRECTION_T;

typedef enum
{
	PT_START= 0,
	PT_END,
    PT_MIDDLE    
} MOTOR_POSITION_T;

/* Includes ------------------------------------------------------------------*/

int do_clean(void *arg);
int do_manage_start(void *arg);
int do_manage_finish(void *arg);

int pt_test(void *arg);
int motor_calibration(void *arg);


#ifdef __cplusplus
}
#endif

#endif /* _MOTOR_H__ */
