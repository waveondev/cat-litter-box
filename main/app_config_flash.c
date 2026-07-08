#include "main.h"

static const char *TAG = __FILE__;

app_config_t app_config = 
{
//    .op_mode = OP_MODE_NORMAL,
    .pump_clean_duration = 180,
    .filter_life_days = 200,
    .splash_delta_g = 50,
    .gate_way_rssi_th = -55,
    .hx1_scale = 1000.0f,
    .hx1_offset = 0,
    .case_raw_data = 0,
    .tof_sense_threshold_l = 250,
    .tof_sense_threshold_r = 250,
    .motion_data_time = 1800,
};

wifi_info_t wifi_info = 
{
   .conn_ssid = "",
   .conn_password = ""
};

ble_info_t ble_info = 
{
   .ble_device_name = ""
};


static bool app_save_flag = false;
static bool wifi_save_flag = false;
static bool ble_save_flag = false;

void app_nvs_save_set(void)
{
    app_save_flag = true;
}
void wifi_nvs_save_set(void)
{
    wifi_save_flag = true;
}
void ble_nvs_save_set(void)
{
    ble_save_flag = true;
}
void reset_all_nvs_data(void)
{
    ESP_LOGW("NVS", "NVS 영역을 포맷(초기화)합니다...");
    
    // ?? 이 함수를 실행하면 NVS 스토리지 전체가 싹 다 포맷됩니다.
    esp_err_t err = nvs_flash_erase();
    
    if (err == ESP_OK) {
        ESP_LOGI("NVS", "포맷 완료! 변경사항을 적용하기 위해 재부팅합니다.");
        esp_restart(); // ?? 중요: 깨끗해진 NVS 구조를 새로 올리기 위해 재부팅 필수
    } else {
        ESP_LOGE("NVS", "NVS 포맷 실패: %s", esp_err_to_name(err));
    }
}

void dump_all_configurations(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "         [SYSTEM CONFIGURATION DUMP]              ");
    ESP_LOGI(TAG, "==================================================");

    // 1. APP 설정 출력
    ESP_LOGI(TAG, "[APP CONFIG]");
//    ESP_LOGI(TAG, "  - Operation Mode       : %ld", app_config.op_mode);
    ESP_LOGI(TAG, "  - Pump Clean Duration  : %ld sec", app_config.pump_clean_duration);
    ESP_LOGI(TAG, "  - Filter Life Days     : %ld days", app_config.filter_life_days);
    ESP_LOGI(TAG, "  - Min Weight Threshold : %ld g", app_config.min_weight_threshold); // 구조체 멤버에 있으면 출력
    ESP_LOGI(TAG, "  - Splash Delta         : %ld g", app_config.splash_delta_g);
    ESP_LOGI(TAG, "  - Gateway RSSI Thr     : %ld dBm", app_config.gate_way_rssi_th);
    ESP_LOGI(TAG, "  - HX1 Scale Factor     : %.2f", app_config.hx1_scale);
    ESP_LOGI(TAG, "  - HX1 Tare Offset      : %ld", app_config.hx1_offset);
    ESP_LOGI(TAG, "  - tof_sense_threshold_l: %ld", app_config.tof_sense_threshold_l);
    ESP_LOGI(TAG, "  - tof_sense_threshold_r: %ld", app_config.tof_sense_threshold_r);
    ESP_LOGI(TAG, "  - motion_data_time     : %ld", app_config.motion_data_time);
    ESP_LOGI(TAG, "--------------------------------------------------");

    // 2. Wi-Fi 설정 출력
    ESP_LOGI(TAG, "[WIFI CONFIG]");
    // SSID나 PASSWORD가 비어있으면 [EMPTY]로 센스있게 표기
    ESP_LOGI(TAG, "  - Wi-Fi SSID           : %s", (wifi_info.conn_ssid[0] == '\0') ? "[EMPTY]" : (char*)wifi_info.conn_ssid);
    ESP_LOGI(TAG, "  - Wi-Fi Password       : %s", (wifi_info.conn_password[0] == '\0') ? "[EMPTY]" : "********"); // 보안상 별표 표기 (원하시면 %s로 생자로 까셔도 됩니다)
    ESP_LOGI(TAG, "--------------------------------------------------");

    // 3. BLE 설정 출력
    ESP_LOGI(TAG, "[BLE CONFIG]");
    ESP_LOGI(TAG, "  - BLE Device Name      : %s", (ble_info.ble_device_name[0] == '\0') ? "[EMPTY]" : (char*)ble_info.ble_device_name);
    
    ESP_LOGI(TAG, "==================================================");
}

app_config_t* get_app_config(void)
{
    return &app_config;
}

wifi_info_t* get_wifi_config(void)
{
    return &wifi_info;
}

ble_info_t* get_ble_config(void)
{
    return &ble_info;
}

void load_app_configuration(void)
{
    // 1. NVS에서 시스템 구조체 통째로 읽어오기 시도
    esp_err_t err = read_nvs_blob(APP_NAMESPACE, APP_CONFIGURATION, &app_config, sizeof(app_config_t));
    
    if (err != ESP_OK) {
        // 2. 만약 최초 부팅이라 데이터가 없다면 기본값(Default) 세팅
        ESP_LOGI(TAG,"[CONFIG] not saved data, create defualt values \r\n");
                
        // 기본값 세팅 후 NVS에 최초로 구워두기
        write_nvs_blob(APP_NAMESPACE, APP_CONFIGURATION, &app_config, sizeof(app_config_t));
    } else {
//        ESP_LOGI(TAG,"[CONFIG] NVS에서 시스템 설정 로드 성공! (opmode = %d 저울 Offset: %d)\r\n", 
//                          app_config.op_mode, app_config.hx1_offset);
    }
}

// 값이 바뀔 때마다 호출해 줄 저장 함수
static void save_app_configuration(void)
{
    write_nvs_blob(APP_NAMESPACE, APP_CONFIGURATION, &app_config, sizeof(app_config_t));
// 2. 검증을 위해 NVS에서 데이터를 다시 읽어올 임시 그릇 생성
    app_config_t temp_cfg;
    memset(&temp_cfg, 0, sizeof(app_config_t)); // 0으로 깨끗하게 청소

    // 3. NVS에서 방금 저장한 값을 다시 로드(Load)
    esp_err_t err = read_nvs_blob(APP_NAMESPACE, APP_CONFIGURATION, &temp_cfg, sizeof(app_config_t));

    if (err == ESP_OK) {
        // 4. memcmp로 원본(app_config)과 NVS에서 읽어온 값(temp_cfg)을 비교
        // 두 메모리 블록이 100% 일치하면 0을 리턴합니다.
        if (memcmp(&app_config, &temp_cfg, sizeof(app_config_t)) == 0) {
            ESP_LOGI(TAG, "[CONFIG] NVS data verification OK! matched 100%%!");
//            ESP_LOGI(TAG, "[CONFIG] 로드된 모드: %ld, 저울 Offset: %ld", 
//                      temp_cfg.op_mode, temp_cfg.hx1_offset);
        } else {
            // 플래시 메모리 물리적 손상이나 섹터 오류 시 감지됨
            ESP_LOGE(TAG, "[CONFIG] NVS data verification failed! mismatched!");
        }
    } else {
        ESP_LOGE(TAG, "[CONFIG] error occured while reading data for verification (%s)", esp_err_to_name(err));
    }
}

void load_wifi_configuration(void)
{
    // 1. NVS에서 시스템 구조체 통째로 읽어오기 시도
    esp_err_t err = read_nvs_blob(APP_NAMESPACE, APP_KEY_WIFI_CONFIG, &wifi_info, sizeof(wifi_info_t));
    
    if (err != ESP_OK) {
        // 2. 만약 최초 부팅이라 데이터가 없다면 기본값(Default) 세팅
        ESP_LOGI(TAG,"[WIFI] no previous saved data, create a new default values.\r\n");
                
        // 기본값 세팅 후 NVS에 최초로 구워두기
        write_nvs_blob(APP_NAMESPACE, APP_KEY_WIFI_CONFIG, &wifi_info, sizeof(wifi_info_t));
    } else {
        ESP_LOGI(TAG,"[WIFI] NVS system load success! (ssid = %s, pass = %s)\r\n", 
                          wifi_info.conn_ssid, wifi_info.conn_password);
    }
}

// 값이 바뀔 때마다 호출해 줄 저장 함수
static void save_wifi_configuration(void)
{
    write_nvs_blob(APP_NAMESPACE, APP_KEY_WIFI_CONFIG, &wifi_info, sizeof(wifi_info_t));
// 2. 검증을 위해 NVS에서 데이터를 다시 읽어올 임시 그릇 생성
    wifi_info_t temp_cfg;
    memset(&temp_cfg, 0, sizeof(wifi_info_t)); // 0으로 깨끗하게 청소

    // 3. NVS에서 방금 저장한 값을 다시 로드(Load)
    esp_err_t err = read_nvs_blob(APP_NAMESPACE, APP_KEY_WIFI_CONFIG, &temp_cfg, sizeof(wifi_info_t));

    if (err == ESP_OK) {
        // 4. memcmp로 원본(wifi_info)과 NVS에서 읽어온 값(temp_cfg)을 비교
        // 두 메모리 블록이 100% 일치하면 0을 리턴합니다.
        if (memcmp(&wifi_info, &temp_cfg, sizeof(wifi_info_t)) == 0) {
            ESP_LOGI(TAG, "[WIFI] NVS data verification success! matched 100%% ");
            ESP_LOGI(TAG, "[WIFI] loaded SSID: %s", temp_cfg.conn_ssid);
        } else {
            // 플래시 메모리 섹터 불량이나 마스킹 오류 시 감지됨
            ESP_LOGE(TAG, "[WIFI] NVS data verification failed, mismatched!");
        }
    } else {
        ESP_LOGE(TAG, "[WIFI] error occure while reading data for verification (%s)", esp_err_to_name(err));
    }
}


void load_ble_configuration(void)
{
    // 1. NVS에서 시스템 구조체 통째로 읽어오기 시도
    esp_err_t err = read_nvs_blob(APP_NAMESPACE, APP_KEY_BLE_CONFIG, &ble_info, sizeof(ble_info_t));
    
    if (err != ESP_OK) {
        // 2. 만약 최초 부팅이라 데이터가 없다면 기본값(Default) 세팅
        ESP_LOGI(TAG,"[BLE] no saved data, create default values \r\n");
                
        // 기본값 세팅 후 NVS에 최초로 구워두기
        write_nvs_blob(APP_NAMESPACE, APP_KEY_BLE_CONFIG, &ble_info, sizeof(ble_info_t));
    } else {
        ESP_LOGI(TAG,"[BLE] NVS system setting loaded success! (ssid = %s, pass = %s)\r\n", 
                          wifi_info.conn_ssid, wifi_info.conn_password);
    }
}

// 값이 바뀔 때마다 호출해 줄 저장 함수
static void save_ble_configuration(void)
{
    write_nvs_blob(APP_NAMESPACE, APP_KEY_BLE_CONFIG, &ble_info, sizeof(ble_info_t));
// 2. 검증을 위해 NVS에서 방금 저장한 값을 다시 읽어올 임시 그릇 생성
    ble_info_t temp_cfg;
    memset(&temp_cfg, 0, sizeof(ble_info_t)); // 깨끗하게 청소

    // 3. NVS에서 데이터를 다시 역으로 로드(Load)
    esp_err_t err = read_nvs_blob(APP_NAMESPACE, APP_KEY_BLE_CONFIG, &temp_cfg, sizeof(ble_info_t));

    if (err == ESP_OK) {
        // 4. ?? memcmp로 원본(ble_info)과 읽어온 것(temp_cfg)을 크기만큼 비교
        // memcmp는 두 메모리가 완전히 일치하면 '0'을 반환합니다.
        if (memcmp(&ble_info, &temp_cfg, sizeof(ble_info_t)) == 0) {
            ESP_LOGI(TAG, "[BLE] NVS data verification success! matched 100%%");
            ESP_LOGI(TAG, "[BLE] loaded name: %s", temp_cfg.ble_device_name);
        } else {
            // 메모리가 일치하지 않는 경우 (대개 이런 일은 거의 없지만, 플래시 불량 등의 이슈 체크용)
            ESP_LOGE(TAG, "[BLE] NVS data verification failed, mismatched");
        }
    } else {
        ESP_LOGE(TAG, "[BLE] error occur while reading data for verification (%s)", esp_err_to_name(err));
    }
}
#define FLASH_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 3)

static void flash_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting flash_task ");

    while (1) {
        if(app_save_flag)
        {
            app_save_flag = false;
            save_app_configuration();
        }
        if(wifi_save_flag)
        {
            wifi_save_flag = false;
            save_wifi_configuration();
        }
        if(ble_save_flag)
        {
            ble_save_flag = false;
            save_ble_configuration();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void NVS_Flash_init(void)
{
    TaskHandle_t xHandle = NULL;
    static uint8_t ucParameterToPass;
    // xTaskCreate 대신 xTaskCreatePinnedToCore를 사용합니다.
    if (xTaskCreatePinnedToCore(
            flash_task,                  // 태스크 함수
            "flash_task",                // 태스크 이름
            FLASH_TASK_STACK_SIZE,       // 스택 크기
            &ucParameterToPass,        // 파라미터
            tskIDLE_PRIORITY + 1,      // 우선순위
            &xHandle,                  // 태스크 핸들
            1                          // ? 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
        
        ESP_LOGE(TAG, "Error creating Button_task on Core 1");
    }
    load_app_configuration();
    load_wifi_configuration();
    load_ble_configuration();
    dump_all_configurations();
    
}
