#ifndef _MAIN_H__
#define _MAIN_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_bt.h"
#include "nvs_flash.h"
#include "led_strip.h"

// 🛠️ ADC 연속 모드 드라이버 헤더
#include "esp_adc/adc_continuous.h"

// 🛠️ MCPWM 드라이버 헤더
#include "driver/mcpwm_timer.h"
#include "driver/mcpwm_oper.h"
#include "driver/mcpwm_cmpr.h"
#include "driver/mcpwm_gen.h"

#include "motor.h"
#include "sensor.h"

#define FEATURE_TOF

// 🌟 부팅 후 IDLE 상태 진입 및 LED 켜짐 완료 플래그 (키 차단 해제용)
extern volatile bool g_is_system_ready;

#define ADC_CHANNEL_1           ADC_CHANNEL_1
#define ADC_CHANNEL_2           ADC_CHANNEL_2
#define ADC_CHANNEL_3           ADC_CHANNEL_3
#define ADC_CHANNEL_6           ADC_CHANNEL_6
#define ADC_CHANNEL_9           ADC_CHANNEL_9

#define PLATE_FAULT             (1 << 0)
#define SCP_INOUT_FAULT         (1 << 1)
#define SCP_SPIN_FAULT          (1 << 2)
#define MAIN_FAULT              (1 << 3)
#define WASTE_FAULT             (1 << 4)

#define UART_BUF_SIZE           (512)
#ifndef PARSE_BUF_SIZE
#define PARSE_BUF_SIZE          (128)
#endif

#define FW_PRJ_NAME             "CAT_LITTER_BOX"
#define FW_HW_REV               1
#define FW_VER_MAJOR            1
#define FW_VER_MINOR            0
#define FW_VER_PATCH            0
#define FW_VER_TIME             __TIME__
#define FW_VER_DATE             __DATE__
#define OTA_URL                 ""

typedef struct {
    int reset_reason;
    uint32_t MIN_VALID_WASTE_RAW;
    uint32_t CLUMPING_WAIT_MIN;
    uint32_t m1_jam_current;
    uint32_t m2_jam_current;
    uint32_t m3_jam_current;
    uint32_t m4_jam_current;
    uint32_t m5_jam_current;
    uint32_t CAT_ENTRY_MIN_WEIGHT;
    uint32_t WASTE_TYPE_RATIO_TH;
    uint32_t EFFECTIVE_DWELL_TIME;
    int gate_way_rssi_th;
    uint32_t tof_sense_threshold_l;
    uint32_t tof_sense_threshold_r;
    uint32_t motion_data_time;
} app_config_t;

typedef enum {
    SCPSPIN_MOTOR = 0,
    WASTE_COVER_MOTOR,
    STEP_MOTOR_MAX
} step_motor_t;

typedef enum {
    PLATE_MOTOR = 0,
    MAIN_COVER_MOTOR,
    SCPINOUT_MOTOR,
    DC_MOTOR_MAX
} dc_motor_t;

typedef enum {
    LOADCELL_MAIN = 0,
    LOADCELL_WASTE,
    LOADCELL_MAX
} loadcell_t;

typedef enum {
    LOADCELL_TARE_CMD = 0,
    LOADCELL_SAVE_STATE1_CMD,
    LOADCELL_SAVE_STATE2_CMD,
    LOADCELL_SAVE_STATE3_CMD,
    LOADCELL_CMD_MAX
} LOADCELL_CMD_T;

enum {
    REASON_NORMAL = 0,
    REASON_DIAG,
    REASON_OTA,
    REASON_FACTORY_RESTORE
};

typedef enum {
    DIAG_IDLE_CMD = 0,
    DIAG_NEXT_STEP_CMD,
    DIAG_NEXT_FUNC_CMD,
    DIAG_CMD_MAX
} DIAG_CMD_T;

typedef enum {
    DIAG_SENSOR_RF_MODE = 0,
    DIAG_PLATE_MODE,
    DIAG_SCPINOUT_MODE,
    DIAG_SCPSPIN_MODE,
    DIAG_WASTE_MODE,
    DIAG_MODE_MAX
} DIAG_MODE_T;

typedef enum {
    PROXI_MODE_IDLE = 0,
    PROXI_MODE_USE,
    PROXI_MODE_ESCAPE,
    PROXI_MODE_HARDEN,
    PROXI_MODE_MAX
} PROXI_MODE_T;

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
    LED_PAUSE_CMD,
    LED_CMD_MAX
} LED_CMD_T;

enum {
    LED_IDLE_MODE = 0,
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
    LED_PAUSE_MODE,
    LED_PAUSE_MODE_1,
    LED_PAUSE_MODE_2,
    LED_MODE_MAX
};

typedef enum {
    UI_CLEAN_CMD = 0,
    UI_MANAGE_FINISH_CMD,
    UI_TARE_ZERO_CMD,
    UI_MANAGE_START_CMD,
    UI_PAIRING_CMD,
    UI_FACTORY_CMD,
    UI_CMD_MAX
} UI_CMD_T;

typedef struct {
    uint32_t task_id;
    uint32_t cmd;
} message_t;

extern volatile bool g_start_ota_flag;

void send_led_cmd_msg(void *message, uint32_t cmd);
void send_ui_cmd_msg(void *message, uint32_t cmd);
void send_loadcell_msg(void *message, uint32_t cmd);
void send_diag_cmd_msg(void *message, uint32_t cmd);

void send_plate_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout);
void send_scpinout_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout);
void send_waste_motor_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout);
void send_scpspin_motor_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout);
void send_scpspin_motor_msg_ex(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t speed, uint32_t timeout, bool cal);
void send_main_motor_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t speed);

bool get_dcmotor_run(dc_motor_t mt);
bool get_dcmotor_dir(dc_motor_t mt);
bool get_stepmotor_run(step_motor_t mt);
bool get_stepmotor_dir(step_motor_t mt);
int get_pt_status(void);
int get_scpspin_cnt(void);

int set_tof_sensor_enable(bool enable);
int sensor_data_parser(char *input);
void loadcell_proc(int *values);
double get_weight(int type);

void led_init(void);
void uv_led_init(void);
void dc_motor_init(void);
void step_motor_init(void);
void current_monitor_init(void);
void ui_init(void);
void keyscan_init(void);
void diag_init(void);

void NVS_Flash_init(void);

bool get_keyclean_status(void);
void set_status_diag(bool status);
bool get_status_diag(void);

app_config_t *get_app_config(void);
void app_nvs_save_set(void);
void system_reset(int reason);

int motor_calibration(void); 
int do_clean_resume_recovery(void);

int nimble_port_stop(void);
void nimble_port_deinit(void);
void ota_main(const char *url);

#endif /* _MAIN_H__ */