#ifndef __STEP_MOTOR_CONTROL_H__
#define __STEP_MOTOR_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	MAIN_IDLE_MODE = 0,
	MAIN_WAIT_MODE,
	MAIN_MODE_MAX
} main_motor_mode_t;

typedef enum {
	WASTE_IDLE_MODE = 0,
	WASTE_WAIT_MODE,
	WASTE_MODE_MAX
} waste_motor_mode_t;

typedef enum {
	SCPSPIN_IDLE_MODE = 0,
	SCPSPIN_WAIT_MODE,
	SCPSPIN_MODE_MAX
} scpspin_motor_mode_t;

typedef enum {
	MAIN_COVER_MOTOR = 0,
	WASTE_COVER_MOTOR,
	SCPSPIN_MOTOR,
	STEP_MOTOR_MAX
} step_motor_t;

/* Includes ------------------------------------------------------------------*/

bool get_stepmotor_run(step_motor_t mt);
int get_scpspin_cnt(void);


void send_main_motor_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout);
void send_waste_motor_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout);
void send_scpspin_motor_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout);

void step_motor_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __STEP_MOTOR_CONTROL_H__ */

