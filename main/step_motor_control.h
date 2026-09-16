#ifndef __STEP_MOTOR_CONTROL_H__
#define __STEP_MOTOR_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

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
	WASTE_COVER_MOTOR = 0,
	SCPSPIN_MOTOR,
	STEP_MOTOR_MAX
} step_motor_t;

/* Includes ------------------------------------------------------------------*/
int get_waste_step_cnt(void);

bool get_stepmotor_run(step_motor_t mt);
bool get_stepmotor_dir(step_motor_t mt);
int get_scpspin_cnt(void);
int get_scpspin_remain_cnt(void);


void send_waste_motor_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout);
void send_scpspin_motor_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout);

void step_motor_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __STEP_MOTOR_CONTROL_H__ */

