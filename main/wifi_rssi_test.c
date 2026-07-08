#include "main.h"

#ifdef FEATURE_WIFI_RSSI_TEST
// ??? 접속할 Wi-Fi AP 정보 입력
#define WIFI_SSID      "WAVEON_2.4G"
#define WIFI_PASSWORD  "1qaz2wsx^^"

static const char *TAG = "WIFI_TEST";
static bool s_is_connected = false;

// Wi-Fi 이벤트 핸들러
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_is_connected = false;
        ESP_LOGW(TAG, "Wi-Fi disconnect retry to connect...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP allocated: " IPSTR, IP2STR(&event->ip_info.ip));
        s_is_connected = true;
    }
}

// 실시간 RSSI 모니터링 태스크
void wifi_connected_rssi_task(void *arg) {
    int rssi = 0;

    while (1) {
        if (s_is_connected) {
            // 현재 연결된 AP의 RSSI 값 가져오기
            esp_err_t ret = esp_wifi_sta_get_rssi(&rssi);
            
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "current AP: [%s] | RSSI: %d dBm", WIFI_SSID, rssi);
                
                // RSSI 세기 기준 가이드 출력
                if (rssi >= -50) {
                    ESP_LOGI(TAG, "signal status : very good (Excellent)");
                } else if (rssi >= -70) {
                    ESP_LOGI(TAG, "signal status : (Good)");
                } else if (rssi >= -80) {
                    ESP_LOGW(TAG, "signal status : (Fair)");
                } else {
                    ESP_LOGE(TAG, "signal status : Bad / likely to disconnect (Poor)");
                }
            } else {
                ESP_LOGE(TAG, "can't get RSSI data (%s)", esp_err_to_name(ret));
            }
        } else {
            ESP_LOGI(TAG, "Wi-Fi wait connecting ...");
        }

        // 2초마다 RSSI 측정 및 출력
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void wifi_test_init(void) 
{
	ESP_LOGI(TAG, "%s +", __func__);
    // 1. NVS 초기화
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. 네트워크 및 이벤트 루프 초기화
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 3. Wi-Fi 초기화
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 4. 이벤트 핸들러 등록
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // 5. Wi-Fi 연결 정보 설정
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // 최소 보안 레벨
        },
    };

    // 6. Wi-Fi 시작 및 구동
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 7. 실시간 RSSI 모니터링 태스크 생성
    xTaskCreate(wifi_connected_rssi_task, "wifi_connected_rssi_task", 4096, NULL, 5, NULL);
}
#endif
