#ifndef _UI_H__
#define _UI_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

typedef enum {
    UI_CLEAN_CMD = 0,
    UI_MANAGE_FINISH_CMD,
    UI_TARE_ZERO_CMD,
    UI_MANAGE_START_CMD,
    UI_PAIRING_CMD,
    UI_FACTORY_CMD,
    UI_CMD_MAX
} ui_cmd_t;


void send_ui_cmd_msg(void *message, uint32_t cmd);

void ui_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _UI_H__ */
