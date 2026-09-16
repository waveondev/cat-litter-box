#ifndef __LOADCELL_H__
#define __LOADCELL_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    LOADCELL_TARE_CMD = 0x01,
    LOADCELL_SAVE_STATE1_CMD, // T: 기구물 미장착
    LOADCELL_SAVE_STATE2_CMD, // U: 링 구조 장착
    LOADCELL_SAVE_STATE3_CMD  // V: 상단 판 장착
} LOADCELL_CMD_T;

typedef enum
{
    LOADCELL_MAIN = 0x01,
    LOADCELL_WASTE
} LOADCELL_SEL_T;

/* Includes ------------------------------------------------------------------*/
double get_weight(int mode);
void send_loadcell_msg(void *message, uint32_t cmd);
void loadcell_proc(int *values);
void loadcell_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __LOADCELL_H__ */
