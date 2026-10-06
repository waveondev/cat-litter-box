#include "app_nvs.h"          // write_nvs_registration_flag() 선언용
#include "rx_mqtt.h"
#include "topic_list.h"
#include "mqtt_operations.h"
#include "stdio.h"
#include "esp_mac.h"
#include "cJSON.h"
#include "device_config.h"    // CONFIG_DEVICE_PREFIX 사용
#include "esp_log.h"

// 💡 전처리 순서 상관없이 암시적 선언 에러 방지를 위한 명시적 extern 선언 추가
extern void write_nvs_registration_flag(bool done);
extern void read_nvs_registration_flag(bool *done);

static const char *TAG = __FILE__;

static uint8_t topic_count = 0;
static char* topic_str[30];

// 외부 BLE 암호화 전송 함수 선언
extern void ble_send_encrypted_event(const char *event_type, const char *plain_data);

char* topic_copy(char* str)
{
    if (topic_count >= 30)
        return NULL;

    uint8_t count = topic_count;
    size_t len = strlen(str);    
    topic_str[topic_count] = (char*)malloc(len + 1);   
    if (topic_str[topic_count] == NULL)
        return NULL;
    memcpy(topic_str[topic_count], str, len + 1);
    topic_count++;
    return topic_str[count];
}

bool mqtt_subscribe_init(void)
{
    char sub_topic[100];
    uint8_t mac_byte[6];
    char dynamicMacStr[13]; // 12자리 MAC 문자열 + 널 종료 문자(\0)
    const char * subTopic;
    bool subStatus;

    // ESP32의 기본 Wi-Fi MAC 주소를 읽어옵니다.
    esp_read_mac(mac_byte, ESP_MAC_WIFI_STA);
    
    snprintf(dynamicMacStr, sizeof(dynamicMacStr), "%02X%02X%02X%02X%02X%02X",
            mac_byte[0], mac_byte[1], mac_byte[2], mac_byte[3], mac_byte[4], mac_byte[5]);

    if (topic_count != 0)
    {
        for (int i = 0; i < topic_count; i++)
            free(topic_str[i]);
        topic_count = 0;
    }

    // 1. REGISTRATION 구독
    snprintf(sub_topic, sizeof(sub_topic), SERVER_RX_TOPIC_REGISTRATION, dynamicMacStr);
    subTopic = topic_copy(sub_topic);
    if (subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s", topic_count, subTopic);
        subStatus = SubscribeToTopic(subTopic, strlen(subTopic));
        if (!subStatus) ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
    }

    // 2. BOOT 구독
    snprintf(sub_topic, sizeof(sub_topic), SERVER_RX_TOPIC_BOOT, dynamicMacStr);
    subTopic = topic_copy(sub_topic);
    if (subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s", topic_count, subTopic);
        subStatus = SubscribeToTopic(subTopic, strlen(subTopic));
        if (!subStatus) ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
    }

    // 3. ACCESS 구독
    snprintf(sub_topic, sizeof(sub_topic), SERVER_RX_TOPIC_ACCESS, dynamicMacStr);
    subTopic = topic_copy(sub_topic);
    if (subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s", topic_count, subTopic);
        subStatus = SubscribeToTopic(subTopic, strlen(subTopic));
        if (!subStatus) ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
    }

    // 4. USAGE (C-100 전용) 구독
    snprintf(sub_topic, sizeof(sub_topic), SERVER_RX_TOPIC_USAGE, dynamicMacStr);
    subTopic = topic_copy(sub_topic);
    if (subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s", topic_count, subTopic);
        subStatus = SubscribeToTopic(subTopic, strlen(subTopic));
        if (!subStatus) ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
    }

    // 5. CLEAN_RESULT (C-100 전용) 구독
    snprintf(sub_topic, sizeof(sub_topic), SERVER_RX_TOPIC_CLEAN_RESULT, dynamicMacStr);
    subTopic = topic_copy(sub_topic);
    if (subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s", topic_count, subTopic);
        subStatus = SubscribeToTopic(subTopic, strlen(subTopic));
        if (!subStatus) ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
    }

    // 6. DIAGNOSTICS 구독
    snprintf(sub_topic, sizeof(sub_topic), SERVER_RX_TOPIC_DIAGNOSTICS, dynamicMacStr);
    subTopic = topic_copy(sub_topic);
    if (subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s", topic_count, subTopic);
        subStatus = SubscribeToTopic(subTopic, strlen(subTopic));
        if (!subStatus) ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
    }

    // 7. HEALTH 구독
    snprintf(sub_topic, sizeof(sub_topic), SERVER_RX_TOPIC_HEALTH, dynamicMacStr);
    subTopic = topic_copy(sub_topic);
    if (subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s", topic_count, subTopic);
        subStatus = SubscribeToTopic(subTopic, strlen(subTopic));
        if (!subStatus) ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
    }

    return true;
}

// 백엔드 등록 성공(registration result="ok") 수신 시 콜백 함수 (온보딩 P5)
static void registration_callback(cJSON* result)
{
    if (result && cJSON_IsString(result) && strcmp(result->valuestring, "ok") == 0) 
    {
        ESP_LOGI(TAG, "[BLE_SEC] 백엔드 등록 성공 수신 완료! 앱에 prov_complete 전송");

        // NVS에 프로비저닝/등록 완료 플래그 기록
        write_nvs_registration_flag(true);

        // MAC 주소를 동적으로 획득하여 thing_name 조립
        uint8_t mac_byte[6];
        char dynamicMacStr[13];
        esp_read_mac(mac_byte, ESP_MAC_WIFI_STA);
        snprintf(dynamicMacStr, sizeof(dynamicMacStr), "%02X%02X%02X%02X%02X%02X",
                mac_byte[0], mac_byte[1], mac_byte[2], mac_byte[3], mac_byte[4], mac_byte[5]);

        char payload_buf[128];
        snprintf(payload_buf, sizeof(payload_buf), "{\"thing_name\":\"%s_%s\"}", CONFIG_DEVICE_PREFIX, dynamicMacStr);

        // 앱으로 AES-GCM 암호화 최종 완료 통보 전송
        ble_send_encrypted_event("prov_complete", payload_buf);
    }
    else
    {
        ESP_LOGI(TAG, "[BLE_SEC] 백엔드 등록 실패");
    }
}

static void boot_callback(cJSON* result)
{
    if (result && cJSON_IsString(result) && strcmp(result->valuestring, "ok") == 0) 
    {
        ESP_LOGI(TAG, "boot ok");
    }
    else
    {
        ESP_LOGI(TAG, "boot result: %s", result ? result->valuestring : "null");
    }
}

static void access_callback(cJSON* result)
{
    if (result && cJSON_IsString(result) && strcmp(result->valuestring, "ok") == 0) 
    {
        ESP_LOGI(TAG, "access ok");
    }
    else
    {
        ESP_LOGI(TAG, "access result: %s", result ? result->valuestring : "null");
    }
}

static void usage_callback(cJSON* result)
{
    if (result && cJSON_IsString(result) && strcmp(result->valuestring, "ok") == 0) 
    {
        ESP_LOGI(TAG, "usage ok");
    }
    else
    {
        ESP_LOGI(TAG, "usage result: %s", result ? result->valuestring : "null");
    }
}

static void clean_result_callback(cJSON* result)
{
    if (result && cJSON_IsString(result) && strcmp(result->valuestring, "ok") == 0) 
    {
        ESP_LOGI(TAG, "clean_result ok");
    }
    else
    {
        ESP_LOGI(TAG, "clean_result result: %s", result ? result->valuestring : "null");
    }
}

static void diagnostics_callback(cJSON* result)
{
    if (result && cJSON_IsString(result) && strcmp(result->valuestring, "ok") == 0) 
    {
        ESP_LOGI(TAG, "diagnostics ok");
    }
    else
    {
        ESP_LOGI(TAG, "diagnostics result: %s", result ? result->valuestring : "null");
    }
}

static void health_callback(cJSON* result)
{
    if (result && cJSON_IsString(result) && strcmp(result->valuestring, "ok") == 0) 
    {
        ESP_LOGI(TAG, "health ok");
    }
    else
    {
        ESP_LOGI(TAG, "health result: %s", result ? result->valuestring : "null");
    }
}

void mqtt_rx_messege(MQTTPublishInfo_t * pPublishInfo, uint16_t packetId)
{
    (void)packetId; // 미사용 매개변수 경고 방지

    if (pPublishInfo->pPayload != NULL && pPublishInfo->payloadLength > 0)
    {
        char *temp_payload = malloc(pPublishInfo->payloadLength + 1);
        if (temp_payload != NULL)
        {
            memcpy(temp_payload, pPublishInfo->pPayload, pPublishInfo->payloadLength);
            temp_payload[pPublishInfo->payloadLength] = '\0';

            // JSON 파싱 시작
            cJSON *root = cJSON_Parse(temp_payload);
            if (root != NULL)
            {
                cJSON *event_type = cJSON_GetObjectItem(root, "event_type");
                cJSON *result = cJSON_GetObjectItem(root, "result");
                
                if (cJSON_IsString(event_type) && (event_type->valuestring != NULL) &&
                    cJSON_IsString(result) && (result->valuestring != NULL)) 
                {
                    if (strcmp(event_type->valuestring, "registration") == 0)       
                    {
                        registration_callback(result);
                    }
                    else if (strcmp(event_type->valuestring, "boot") == 0)       
                    {
                        boot_callback(result);
                    }   
                    else if (strcmp(event_type->valuestring, "access") == 0)       
                    {
                        access_callback(result);
                    }      
                    else if (strcmp(event_type->valuestring, "usage") == 0)       
                    {
                        usage_callback(result);
                    }      
                    else if (strcmp(event_type->valuestring, "clean_result") == 0)       
                    {
                        clean_result_callback(result);
                    }      
                    else if (strcmp(event_type->valuestring, "diagnostics") == 0)       
                    {
                        diagnostics_callback(result);
                    }                               
                    else if (strcmp(event_type->valuestring, "health") == 0)       
                    {
                        health_callback(result);
                    }                                                   
                }
                cJSON_Delete(root);
            }
            free(temp_payload);
        }
    }
}