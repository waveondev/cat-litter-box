#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "aws_iot_task.h"
#include "aws_iot_config.h"
#include "wifi_task.h"
#include "mqtt_operations.h"
#include "tx_mqtt.h"

static const char *TAG = "aws_iot_task";
extern EventGroupHandle_t s_wifi_event_group;
static bool is_aws_started = false;
static QueueHandle_t mqtt_tx_queue = NULL;
static QueueHandle_t tracker_mqtt_queue = NULL;
static esp_timer_handle_t Health_timer;

extern int aws_iot_provisioning_main(int argc, char ** argv);

#define TIMER_1_MIN_IN_US    (60ULL * 1000000ULL)
#define TIMER_15_MIN_IN_US   (15ULL * TIMER_1_MIN_IN_US)

static void Health_timer_callback(void* arg)
{
    mqtt_queue_send(MESSEGE_HEALTH, NULL, 0);
    
    if (esp_timer_is_active(Health_timer)) {
        esp_timer_stop(Health_timer);
    } 
    esp_timer_start_once(Health_timer, TIMER_15_MIN_IN_US);
}

bool mqtt_queue_send(messege_tx_mqtt_cmd_e cmd, void* data, uint32_t data_len)
{
    mqtt_packet_t mqtt_packet = {0};
    if (mqtt_tx_queue == NULL) {
        ESP_LOGW("mqtt_tx", "mqtt_tx_queue NULL.");
        return false;        
    }
    
    if (data != NULL && data_len > 0) {
        mqtt_packet.data = calloc(1, data_len);
        if (mqtt_packet.data != NULL) {   
            memcpy(mqtt_packet.data, data, data_len);
        }   
    }

    mqtt_packet.cmd = cmd;
    mqtt_packet.data_len = data_len;

    if (xQueueSend(mqtt_tx_queue, &mqtt_packet, 0) != pdPASS) {
        ESP_LOGW("mqtt_tx", "Queue full! Dropping packet and freeing memory.");
        if (mqtt_packet.data != NULL) free(mqtt_packet.data);
        return false;
    }
    return true;
}

void tracker_mqtt_queue_send(messege_tx_mqtt_cmd_e cmd, uint8_t* mac, Motion_Packet_t* packet, uint32_t data_len, pack_data* data)
{
    tracker_mqtt_packet_t mqtt_packet;

    if (tracker_mqtt_queue == NULL) {
        if (data != NULL) free(data);
        return;        
    }

    memset(&mqtt_packet, 0, sizeof(tracker_mqtt_packet_t));
    mqtt_packet.cmd = cmd;
    memcpy(mqtt_packet.mac, mac, sizeof(mqtt_packet.mac));
    memcpy(&mqtt_packet.packet, packet, sizeof(Motion_Packet_t));
    mqtt_packet.data_len = data_len;
    mqtt_packet.data = data;

    if (xQueueSend(tracker_mqtt_queue, &mqtt_packet, 0) != pdPASS) {
        ESP_LOGW("mqtt_tx", "Queue full! Dropping packet and freeing memory.");
        if (data != NULL) free(data);
    }
}

static void aws_iot_main_entry(void *pvParameters)
{
    ESP_LOGI(TAG, "AWS IoT 전담 태스크 시작");
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    mqtt_tx_queue = xQueueCreate(10, sizeof(mqtt_packet_t));
    tracker_mqtt_queue = xQueueCreate(10, sizeof(tracker_mqtt_packet_t));

    const esp_timer_create_args_t Health_timer_args = {
        .callback = &Health_timer_callback,
        .name = "Health_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&Health_timer_args, &Health_timer));

    int provisioning_count = 0;
    mqtt_packet_t mqtt_packet;
    tracker_mqtt_packet_t tracker_mqtt_packet;

    while (1) {
        xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

        xQueueReset(mqtt_tx_queue);
        xQueueReset(tracker_mqtt_queue);

        while (aws_iot_provisioning_main(0, NULL) != EXIT_SUCCESS) {
            vTaskDelay(pdMS_TO_TICKS(3000));
            provisioning_count++;
            if (provisioning_count > 10) esp_restart();
        }

        is_aws_started = true;
        esp_timer_start_once(Health_timer, TIMER_1_MIN_IN_US);

        for (;;) {
            if (xQueueReceive(mqtt_tx_queue, &mqtt_packet, pdMS_TO_TICKS(10)) == pdTRUE) {
                Send_cJSON_Messege(&mqtt_packet);
                if (mqtt_packet.data != NULL) {
                    free(mqtt_packet.data);
                    mqtt_packet.data = NULL;
                }                
            }
            if (xQueueReceive(tracker_mqtt_queue, &tracker_mqtt_packet, pdMS_TO_TICKS(10)) == pdTRUE) {
                Send_cJSON_Messege_for_tracker(&tracker_mqtt_packet);
                if (tracker_mqtt_packet.data != NULL) {
                    free(tracker_mqtt_packet.data);
                    tracker_mqtt_packet.data = NULL;
                }
            }

            if (ProcessLoopWithTimeout(10) == false) {
                ESP_LOGE(TAG, "MQTT 연결 끊김 감지!");
                break;
            }
        }

        esp_timer_stop(Health_timer);
        DisconnectMqttSession();
        vTaskDelay(10000);
    }
    vTaskDelete(NULL);
}

bool get_aws_started(void)
{
    return is_aws_started;
}

#define AWS_IOT_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 6)
void aws_iot_task_init(void)
{
    if (is_aws_started == false) {
        if (xTaskCreatePinnedToCore(
            aws_iot_main_entry, "aws_iot_task", AWS_IOT_TASK_STACK_SIZE,
            NULL, tskIDLE_PRIORITY + 5, NULL, 1
        ) != pdPASS) {
            ESP_LOGE(TAG, "Error creating aws_iot_task on Core 1");
        }
    }
}