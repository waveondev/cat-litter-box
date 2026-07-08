#ifndef __LED_H__
#define __LED_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
typedef enum {
    LED_IDLE_CMD = 0,
    LED_INITOK_CMD,
    LED_PAIRING_CMD,
    LED_UNLOCK_CMD,
    LED_LOCK_CMD,
    LED_INVALID_CMD,
    LED_INUSE_CMD,
    LED_CLEANING_CMD,
    LED_MANAGE_CMD,
    LED_BINFULL_CMD,
    LED_ERROR_CMD,
    LED_QCMODE_CMD,
    LED_QCQUIT_CMD,
    LED_FULL_RED_CMD,		// test
    LED_FULL_GREEN_CMD,       // test
    LED_FULL_BLUE_CMD,       // test
    LED_FULL_WHITE_CMD,       // test
    LED_FULL_OFF_CMD,       // test
    LED_CMD_MAX
} led_cmd_t;

typedef enum {
    LED_IDLE_MODE,
    LED_NORMAL_MODE,
    LED_INITOK_MODE,
    LED_INITOK_MODE_1,
    LED_INITOK_MODE_2,
    LED_PAIRING_MODE,
    LED_PAIRING_MODE_1,
    LED_PAIRING_MODE_2,
    LED_UNLOCK_MODE,
    LED_UNLOCK_MODE_1,
    LED_UNLOCK_MODE_2,
    LED_UNLOCK_MODE_3,
    LED_UNLOCK_MODE_4,
    LED_LOCK_MODE,
    LED_LOCK_MODE_1,
    LED_LOCK_MODE_2,
    LED_INVALID_MODE,
    LED_INVALID_MODE_1,
    LED_INVALID_MODE_2,
    LED_INVALID_MODE_3,
    LED_INVALID_MODE_4,
    LED_INUSE_MODE,
    LED_CLEANING_MODE,
    LED_CLEANING_MODE_1,
    LED_MANAGE_MODE,
    LED_MANAGE_MODE_1,
    LED_BINFULL_MODE,
    LED_ERROR_MODE,
    LED_ERROR_MODE_1,
    LED_ERROR_MODE_2,
    LED_QCMODE_MODE,
    LED_QCMODE_MODE_1,
    LED_QCMODE_MODE_2,
    LED_QCQUIT_MODE,
    LED_QCQUIT_MODE_1,
    LED_QCQUIT_MODE_2,
    LED_QCQUIT_MODE_3,
    LED_QCQUIT_MODE_4,
    LED_QCQUIT_MODE_5,
    LED_MODE_MAX
} led_mode_t;

void send_led_cmd_msg(void *message, uint32_t cmd);

void led_init(void);



#ifdef __cplusplus
}
#endif

#endif /* __LED_H__ */
