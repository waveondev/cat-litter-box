#ifndef __DC_MOTOR_CONTROL_H__
#define __DC_MOTOR_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

void send_dc_motor_msg(void *message, uint32_t cmd);

void dc_motor_init(void);


#ifdef __cplusplus
}
#endif

#endif /* __DC_MOTOR_CONTROL_H__ */
