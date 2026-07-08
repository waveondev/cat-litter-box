#ifndef __STEP_MOTOR_CONTROL_H__
#define __STEP_MOTOR_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	STEP_IDLE_MODE = 0,
	MAIN_WASTE_WAIT_MODE,
	SCP_SPIN_WAIT_MODE,
	STEP_MODE_MAX
} step_mode_t;


typedef enum {
	MAIN_COVER_MOTOR = 0,
	WASTE_COVER_MOTOR,
	SCP_SPIN_MOTOR,
	STEP_MOTOR_MAX
} step_motor_t;

/* Includes ------------------------------------------------------------------*/

bool get_step_motor_run(step_motor_t mt);

void send_step_motor_msg(void *message, uint32_t cmd);

void step_motor_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __STEP_MOTOR_CONTROL_H__ */

