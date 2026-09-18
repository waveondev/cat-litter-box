#ifndef _MOTOR_H__
#define _MOTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MOTOR_CMD_TRANSFER_DELAY	(50)

typedef struct {
    uint32_t task_id;
    uint32_t cmd;
    uint32_t angle;
    uint32_t direction;
    uint32_t speed;
    uint32_t timeout;
    bool cal;
} mt_message_t;

typedef enum {
	DC_MOTOR = 0,
	STEP_MOTOR,
	
	MOTOR_MAX
} motor_t;

typedef enum {
	MT_OPEN = 0,
	MT_CLOSE,
	MT_MIDDLE,
	MT_STATUS_MAX
} motor_status_t;

typedef enum
{
	WASTE_COVER_CMD = 0x01,
    PLATE_CMD,
    SCP_INOUT_CMD,
    SCP_SPIN_CMD,
    MAIN_COVER_CMD,
	SCP_SPIN_STEP_CMD,
	SCP_SPIN_SPEED_CMD,
	WASTE_STEP_CMD,
    EMERGENCY_RESET_CMD,
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
	MT_RUN= 0,
	MT_STOP,
    MT_RESUME    
} MOTOR_OPERATION_T;


typedef enum
{
	PT_START= 0,
	PT_END,
    PT_MIDDLE    
} MOTOR_POSITION_T;

/* Includes ------------------------------------------------------------------*/
bool check_spin_mt_on(void);
int pt_check(int sel, int mt);

int do_clean(void *arg);
int do_manage_start(void *arg);
int do_manage_finish(void *arg);
int motor_main_cover_test(int dir);
int motor_waste_cover_test(int dir);

void send_motor_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout);
void motor_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _MOTOR_H__ */
