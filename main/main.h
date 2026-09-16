#ifndef __MAIN_H__
#define __MAIN_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_ota_ops.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "driver/mcpwm_prelude.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"
#include "dc_motor_control.h"
#include "step_motor_control.h"
#include "led_strip.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_wifi_types.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "keyscan.h"
#include "uv_led.h"
#include "led.h"
#include "ui.h"
#include "sensor.h"
#include "iot_button.h"
#include "loadcell.h"
#include "diag.h"
#include "nvs.h"

#include "motor.h"
#include "current_monitor.h"
#include "app_config_flash.h"


#define FW_PRJ_NAME                     "[C-100]CAT Litter Box"
#define FW_VER_MAJOR                    0
#define FW_VER_MINOR                    1
#define FW_VER_PATCH                    0

#define FW_HW_REV                       1

#define FW_CC_HIGH                      (0x01)
#define FW_CC_LOW                       (0x9A)

#define FW_VER_DATE                     ""
#define FW_VER_TIME                     ""


#define FEATURE_SENSOR_INPUT

//#define FEATURE_TOF
#define FEATURE_FLATTENING
#define FEATURE_MAIN_COVER
#define FEATURE_SHAKE_SCOOP
#define FEATURE_CLEAN_QUICK_DEMO

#define FEATURE_AWS_IOT
#ifdef FEATURE_AWS_IOT
#include "ble_tracker_id.h"
#include "esp_spiffs.h"
#include "wifi_task.h"
#include "ble_task.h"
#include "aws_iot_task.h"
#include <sys/stat.h>
#include <unistd.h>
#endif

#define UART_BUF_SIZE      (1024)

/* Includes ------------------------------------------------------------------*/

typedef struct {
    uint32_t task_id;
    uint32_t cmd;
} message_t;

unsigned int get_boot_reason(void);
void system_reset(int reason);
unsigned int get_freeheap_size(int line);

// [추가] 긴급 정지 관련 함수 선언
extern void set_emergency_stop(void);
extern void clear_emergency_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H__ */
