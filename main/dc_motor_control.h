#ifndef __DC_MOTOR_CONTROL_H__
#define __DC_MOTOR_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	PLATE_MOTOR = 0,
	SCPINOUT_MOTOR,
	DC_MOTOR_MAX
} dc_motor_t;

/* Includes ------------------------------------------------------------------*/

void send_plate_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout);
void send_scpinout_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout);
void dc_motor_init(void);
bool get_dcmotor_run(dc_motor_t mt);
bool get_dcmotor_dir(dc_motor_t mt);

#ifdef __cplusplus
}
#endif

#endif /* __DC_MOTOR_CONTROL_H__ */
