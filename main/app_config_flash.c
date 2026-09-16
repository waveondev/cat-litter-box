#include "app_config_flash.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = __FILE__;

app_config_t app_config = 
{
	.MIN_VALID_WASTE_RAW = 50, 	
	.CLUMPING_WAIT_MIN = 10,	
    // 기준 전압(3.3V)의 30%에 해당하는 약 495mA로 차단 임계치 변경
	.m1_jam_current = 495,
	.m2_jam_current = 495,
	.m3_jam_current = 495,
	.m4_jam_current = 495,
	.m5_jam_current = 495,

	.CAT_ENTRY_MIN_WEIGHT = 500,
	.WASTE_TYPE_RATIO_TH = 20,	
	.EFFECTIVE_DWELL_TIME = 5,	

	.reset_reason = REASON_NORMAL,
    .gate_way_rssi_th = -85,
    .tof_sense_threshold_l = 250,
    .tof_sense_threshold_r = 250,
    .motion_data_time = 1800,
};

app_wifi_config_t wifi_config = { .conn_ssid = "", .conn_password = "" };
app_ble_config_t ble_config = { .ble_device_name = "" };

static bool app_save_flag = false;
static bool wifi_save_flag = false;
static bool ble_save_flag = false;
static bool motor_save_flag = false;
static uint32_t motor_save_time = 0;

void app_nvs_save_set(void) { app_save_flag = true; }
void wifi_nvs_save_set(void) { wifi_save_flag = true; }
void ble_nvs_save_set(void) { ble_save_flag = true; }
void motor_nvs_save_set(void) { motor_save_flag = true; }

void reset_all_nvs_data(void)
{
    esp_err_t err = nvs_flash_erase();
    if (err == ESP_OK) {
        esp_restart(); 
    } 
}

void dump_all_configurations(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "         [SYSTEM CONFIGURATION DUMP]              ");
    ESP_LOGI(TAG, "==================================================");
}

app_config_t* get_app_config(void) { return &app_config; }
app_wifi_config_t* get_wifi_config(void) { return &wifi_config; }
app_ble_config_t* get_ble_config(void) { return &ble_config; }
uint32_t* get_motor_time(void) { return &motor_save_time; }

void erase_app_configuration(void)
{
    memset(&app_config,0,sizeof(app_config));
    write_nvs_blob(APP_NAMESPACE, APP_KEY_CONFIGURATION, &app_config, sizeof(app_config));
}

void load_app_configuration(void)
{
    esp_err_t err = read_nvs_blob(APP_NAMESPACE, APP_KEY_CONFIGURATION, &app_config, sizeof(app_config_t));
    if (err != ESP_OK) {
        write_nvs_blob(APP_NAMESPACE, APP_KEY_CONFIGURATION, &app_config, sizeof(app_config_t));
    } 
}

static void save_app_configuration(void)
{
    write_nvs_blob(APP_NAMESPACE, APP_KEY_CONFIGURATION, &app_config, sizeof(app_config_t));
}

void erase_wifi_configuration(void)
{
    memset(&wifi_config,0,sizeof(wifi_config));
    write_nvs_blob(APP_NAMESPACE, APP_KEY_WIFI_CONFIG, &wifi_config, sizeof(wifi_config));
}

void load_wifi_configuration(void)
{
    esp_err_t err = read_nvs_blob(APP_NAMESPACE, APP_KEY_WIFI_CONFIG, &wifi_config, sizeof(app_wifi_config_t));
    if (err != ESP_OK) {
        write_nvs_blob(APP_NAMESPACE, APP_KEY_WIFI_CONFIG, &wifi_config, sizeof(app_wifi_config_t));
    } 
}

static void save_wifi_configuration(void)
{
    write_nvs_blob(APP_NAMESPACE, APP_KEY_WIFI_CONFIG, &wifi_config, sizeof(app_wifi_config_t));
}

void erase_ble_configuration(void)
{
    memset(&ble_config,0,sizeof(ble_config));
    write_nvs_blob(APP_NAMESPACE, APP_KEY_BLE_CONFIG, &ble_config, sizeof(ble_config));
}

void load_ble_configuration(void)
{
    esp_err_t err = read_nvs_blob(APP_NAMESPACE, APP_KEY_BLE_CONFIG, &ble_config, sizeof(app_ble_config_t));
    if (err != ESP_OK) {
        write_nvs_blob(APP_NAMESPACE, APP_KEY_BLE_CONFIG, &ble_config, sizeof(app_ble_config_t));
    } 
}

static void save_ble_configuration(void)
{
    write_nvs_blob(APP_NAMESPACE, APP_KEY_BLE_CONFIG, &ble_config, sizeof(app_ble_config_t));
}

void load_motor_time(void)
{
    esp_err_t err = read_nvs_uint(APP_NAMESPACE, APP_KEY_MOTOR_TIME, &motor_save_time);
    if (err != ESP_OK) {
        write_nvs_uint(APP_NAMESPACE, APP_KEY_MOTOR_TIME, motor_save_time);
    } 
}

static void save_motor_time(void)
{
    write_nvs_uint(APP_NAMESPACE, APP_KEY_MOTOR_TIME, motor_save_time);
}

#define FLASH_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

static void flash_task(void *pvParameter)
{
    while (1) {
        if(app_save_flag) { app_save_flag = false; save_app_configuration(); }
        if(wifi_save_flag) { wifi_save_flag = false; save_wifi_configuration(); }
        if(ble_save_flag) { ble_save_flag = false; save_ble_configuration(); }
        if(motor_save_flag) { motor_save_flag = false; save_motor_time(); }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void NVS_Flash_init(void)
{
    TaskHandle_t xHandle = NULL;
    static uint8_t ucParameterToPass;
    xTaskCreatePinnedToCore(
            flash_task, "flash_task", FLASH_TASK_STACK_SIZE,       
            &ucParameterToPass, tskIDLE_PRIORITY + 1, &xHandle, 1);                 
            
    load_app_configuration();
    load_wifi_configuration();
    load_ble_configuration();
    load_motor_time();
    dump_all_configurations();
}

void save_lc_calibration_to_nvs(int state, int *offsets)
{
    char nvs_key[16];
    snprintf(nvs_key, sizeof(nvs_key), "lc_cal_%d", state);
    write_nvs_blob(APP_NAMESPACE, nvs_key, offsets, sizeof(int) * 4);
    ESP_LOGI("NVS_LC", "상태 %d 로드셀 영점 데이터 저장 완료", state);
}

void load_lc_calibration_from_nvs(int state, int *offsets)
{
    char nvs_key[16];
    snprintf(nvs_key, sizeof(nvs_key), "lc_cal_%d", state);
    esp_err_t err = read_nvs_blob(APP_NAMESPACE, nvs_key, offsets, sizeof(int) * 4);
    if (err == ESP_OK) {
        ESP_LOGI("NVS_LC", "상태 %d 로드셀 영점 로드 성공!", state);
    } else {
        ESP_LOGW("NVS_LC", "상태 %d 로드셀 저장 데이터 없음.", state);
    }
}