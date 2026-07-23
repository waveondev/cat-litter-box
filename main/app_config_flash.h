#ifndef __APP_CONFIG_FLASH_H__
#define __APP_CONFIG_FLASH_H__

#include "app_nvs.h"

#define BLE_DEVICENAME_LEN 32
#define WIFI_PASSWORD_LEN 64
typedef struct{
	// dedicated setting value for cat-toilet
	uint32_t MIN_VALID_WASTE_RAW;	// default : 5.0 g
	uint32_t CLUMPING_WAIT_MIN;		// default : 10 minute
	uint32_t JAM_CURRENT_LIMIT;		// default : 1200 mA
	uint32_t CAT_ENTRY_MIN_WEIGHT;	// default : 500 g
	uint32_t WASTE_TYPE_RATIO_TH;	// default : 20
	uint32_t EFFECTIVE_DWELL_TIME;	// default : 5 sec

    int32_t gate_way_rssi_th;
    float hx1_scale;                 // counts per gram
    int32_t hx1_offset;              // tare offset
    uint32_t case_raw_data;
    uint32_t tof_sense_threshold_l;
    uint32_t tof_sense_threshold_r;
    uint32_t motion_data_time;
    char env_mode[16];               // "dev" 또는 "prod" 저장용
    char mqtt_url[128];              // (선택) 서버 주소 저장용
}app_config_t;



typedef struct{
    uint8_t conn_ssid[BLE_DEVICENAME_LEN];
    uint8_t conn_password[WIFI_PASSWORD_LEN];
}app_wifi_config_t;

typedef struct{
    uint8_t ble_device_name[BLE_DEVICENAME_LEN];
}app_ble_config_t;
void reset_all_nvs_data(void);
void app_nvs_save_set(void);
void wifi_nvs_save_set(void);
void ble_nvs_save_set(void);
void motor_nvs_save_set(void);
app_config_t* get_app_config(void);
app_wifi_config_t* get_wifi_config(void);
app_ble_config_t* get_ble_config(void);
uint32_t* get_motor_time(void);

void load_app_configuration(void);

void load_wifi_configuration(void);

void load_ble_configuration(void);

void NVS_Flash_init(void);
void dump_all_configurations(void);
#endif

