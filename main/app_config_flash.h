#ifndef __APP_CONFIG_FLASH_H__
#define __APP_CONFIG_FLASH_H__

#include "app_nvs.h"

#define BLE_DEVICENAME_LEN 32
#define WIFI_PASSWORD_LEN 64

typedef enum {
    REASON_NORMAL = 0,
    REASON_FAULT,
    REASON_FACTORY_RESTORE,
    REASON_OTA,
    REASON_DIAG,
    REASON_TEST,
    REASON_MAX
} reset_reason_t;

typedef struct{
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

    int32_t reset_reason;           
    int32_t gate_way_rssi_th;
    uint32_t tof_sense_threshold_l;
    uint32_t tof_sense_threshold_r;
    uint32_t motion_data_time;
    char env_mode[16];               
    char mqtt_url[128];              
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

// [추가] 로드셀 캘리브레이션 NVS 저장/로드 함수
void save_lc_calibration_to_nvs(int state, int *offsets);
void load_lc_calibration_from_nvs(int state, int *offsets);

#endif
