#ifndef __MAIN_H__
#define __MAIN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_ota_ops.h"
#include "driver/gpio.h"
#include "driver/ledc.h"  // ESP32-S3 ÇÏµå¿þ¾î PWM Á¦¾î Çì´õ
#include "driver/uart.h"
#include "driver/mcpwm_prelude.h" // v5.x ÅëÇÕ MCPWM Çì´õ
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"
#include <esp_https_ota.h>
#include <esp_ota_ops.h>
#include "mqtt_main.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"  // v5.x í˜¸í™˜ìš©
#include "esp_vfs_dev.h"
#include "protocol_examples_common.h"
#include "lwip/apps/mqtt.h"
#include "aws_iot_task.h"
#include "esp_spiffs.h"
#include "dc_motor_control.h"
#include "step_motor_control.h"
#include "led_strip.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_wifi_types.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_mac.h" // MAC ÁÖ¼Ò °ü·Ã API Çì´õ
#include "nvs_flash.h"
#include "keyscan.h"
#include "uv_led.h"
#include "led.h"
#include "wifi_rssi_test.h"
#include "ui.h"
#include "sensor.h"
#include "iot_button.h"
#include "loadcell.h"
#include "motor.h"

#include "app_config_flash.h"
#include "wifi_task.h"
#include "http_ota.h"
#include "ble_task.h"
#include "ble_parse.h"
#include "ble_tracker_id.h"

#define FW_PRJ_NAME						"[C-100]CAT Litter Box"
#define FW_VER_MAJOR					0
#define FW_VER_MINOR					1
#define FW_VER_PATCH					0

#define FW_HW_REV						1

// 410 : KR country code, iso3166
#define FW_CC_HIGH						(0x01)
#define FW_CC_LOW						(0x9A)

#define FW_VER_DATE						__DATE__
#define FW_VER_TIME						__TIME__


//#define FEATURE_WIFI_RSSI_TEST
//#define ENABLE_SENSOR_INPUT
///////////////////////////////////////////////////////////
// demo synario, should select one item
//#define FEATURE_LED_TEST

#define SCP_INPUT_WAIT		(10000)
#define WATE_COVER_WAIT		(13000)

#define UART_BUF_SIZE      (1024)

/* Includes ------------------------------------------------------------------*/

typedef struct {
    uint32_t task_id;
    uint32_t cmd;
} message_t;

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H__ */
