#ifndef __TX_MQTT_H__
#define __TX_MQTT_H__

#include "esp_log.h"
#include "ble_parse.h"

typedef enum {
    MESSEGE_REGISTRATION        = 0x00,
    MESSEGE_BOOT,
    MESSEGE_ACCESS,
    MESSEGE_USAGE,              // C-100 추가
    MESSEGE_CLEAN_RESULT,       // C-100 추가
    MESSEGE_DIAGNOSTICS,
    MESSEGE_HEALTH,

    AWS_MESSEGE_AWS_JOBS_GET    = 0x80,

    TRACKER_MESSEGE_ACTIVITY    = 0xF0,
    TRACKER_MESSEGE_DIAGNOSTICS,
    TRACKER_MESSEGE_HEALTH,
} messege_tx_mqtt_cmd_e;

// W-100 동적 데이터 구조체 (큐 전달용)
typedef struct {
    messege_tx_mqtt_cmd_e cmd;     
    void* data;
    uint32_t data_len;
} mqtt_packet_t;

typedef struct {
    messege_tx_mqtt_cmd_e cmd;     
    uint8_t mac[6];    
    Motion_Packet_t packet;
    pack_data* data;
    uint32_t data_len;
} tracker_mqtt_packet_t;

// C-100 사용(Usage) 패킷 구조체
typedef struct {
    float cat_weight;
    float waste_raw;
    uint32_t duration_sec;
} USAGE_Packet_t;

// C-100 청소 결과(Clean Result) 패킷 구조체
typedef struct {
    float disposed;
    float waste_ratio;
    uint8_t waste_type;
    uint8_t send_level;
} CLEAN_RESULT_Packet_t;

void Send_cJSON_Messege(mqtt_packet_t* mqtt_packet);
void Send_cJSON_Messege_for_tracker(tracker_mqtt_packet_t* tracker_mqtt_packet);

#endif /* __TX_MQTT_H__ */