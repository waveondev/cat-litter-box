#include "tx_mqtt.h"
#include "topic_list.h"
#include "cJSON.h"
#include "esp_mac.h"
#include "mqtt_operations.h"
#include "ble_task.h"
#include "esp_system.h" // esp_reset_reason() 전용 헤더
#include "device_config.h"

static const char *TAG = __FILE__;

Motion_Packet_t motion_res;
Motion_Packet_t health_res;

static cJSON* Get_cJSON_Header(messege_tx_mqtt_cmd_e cmd)
{
    uint32_t current_timestamp = (uint32_t)(esp_log_timestamp() / 1000);
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON_AddStringToObject(root, "id", TEST_UUID);
    if (TRACKER_MESSEGE_ACTIVITY <= cmd) {
        cJSON_AddStringToObject(root, "env", "alpha");
        cJSON_AddStringToObject(root, "device_type", "tracker"); 
    } else {
        cJSON_AddStringToObject(root, "env", "alpha");
        cJSON_AddStringToObject(root, "device_type", CONFIG_DEVICE_TYPE); 
    }
    
    switch (cmd) {
        case MESSEGE_REGISTRATION: cJSON_AddStringToObject(root, "event_type", "registration"); break;
        case MESSEGE_BOOT:         cJSON_AddStringToObject(root, "event_type", "boot"); break;
        case MESSEGE_ACCESS:       cJSON_AddStringToObject(root, "event_type", "access"); break;
        case MESSEGE_USAGE:        cJSON_AddStringToObject(root, "event_type", "usage"); break;
        case MESSEGE_CLEAN_RESULT: cJSON_AddStringToObject(root, "event_type", "clean_result"); break;
        case MESSEGE_DIAGNOSTICS:  cJSON_AddStringToObject(root, "event_type", "diagnostics"); break;
        case MESSEGE_HEALTH:       cJSON_AddStringToObject(root, "event_type", "health"); break;
        case TRACKER_MESSEGE_ACTIVITY: cJSON_AddStringToObject(root, "event_type", "activity"); break;
        case TRACKER_MESSEGE_HEALTH:   cJSON_AddStringToObject(root, "event_type", "health"); break;
        default: cJSON_Delete(root); return NULL;
    }

    if (TRACKER_MESSEGE_ACTIVITY <= cmd) {
        char str[20];
        snprintf(str, sizeof(str), "v%d.%d.%d", health_res.health_data_res.major,
                                                health_res.health_data_res.minor,
                                                health_res.health_data_res.patch);
        cJSON_AddStringToObject(root, "firmware", str);
    } else {
        cJSON_AddStringToObject(root, "firmware", CONFIG_FW_VERSION);
    }

    cJSON_AddNumberToObject(root, "timestamp", current_timestamp);
    return root;
}

static cJSON* Get_cJSON_Data(mqtt_packet_t* mqtt_packet)
{
    cJSON *data_obj = cJSON_CreateObject();
    uint8_t mac_byte[6];
    char dynamicMacStr[13];
    cJSON *subsystems;
    cJSON *sensor_obj;

    if (data_obj == NULL) return NULL;

    switch (mqtt_packet->cmd) {
        case MESSEGE_REGISTRATION:
            esp_read_mac(mac_byte, ESP_MAC_WIFI_STA);
            snprintf(dynamicMacStr, sizeof(dynamicMacStr), "%02X%02X%02X%02X%02X%02X",
                    mac_byte[0], mac_byte[1], mac_byte[2], mac_byte[3], mac_byte[4], mac_byte[5]);

            cJSON_AddStringToObject(data_obj, "mac", dynamicMacStr);
            cJSON_AddStringToObject(data_obj, "hw_rev", CONFIG_HW_REV);
            cJSON_AddNumberToObject(data_obj, "paired_at", 1747396800);
            break;

        case MESSEGE_BOOT:
            cJSON_AddStringToObject(data_obj, "boot_reason", "power_on");
            cJSON_AddNumberToObject(data_obj, "uptime_sec", esp_log_timestamp() / 1000);
            cJSON_AddStringToObject(data_obj, "power_source", "ADAPTER");
            cJSON_AddNumberToObject(data_obj, "reset_reason", esp_reset_reason());
            break;

        case MESSEGE_ACCESS:
            cJSON_AddStringToObject(data_obj, "access_id", TEST_UUID);
            cJSON_AddStringToObject(data_obj, "source", get_tracker_found() ? "tracker" : "sensor");
            cJSON_AddStringToObject(data_obj, "beacon_id", "TRACKER_112233445566");
            cJSON_AddNumberToObject(data_obj, "rssi_dbm", 0);
            break;

        case MESSEGE_USAGE:
            if (mqtt_packet->data != NULL) {
                USAGE_Packet_t* usage = (USAGE_Packet_t*)mqtt_packet->data;
                cJSON_AddStringToObject(data_obj, "usage_id", TEST_UUID);
                cJSON_AddStringToObject(data_obj, "session_type", get_tracker_found() ? "normal" : "unknown");
                cJSON_AddStringToObject(data_obj, "access_id_refs", "");
                cJSON_AddStringToObject(data_obj, "tracker_id", "TRACKER_112233445566");
                cJSON_AddNumberToObject(data_obj, "cat_weight", usage->cat_weight);
                cJSON_AddNumberToObject(data_obj, "waste_raw", usage->waste_raw);
                cJSON_AddNumberToObject(data_obj, "duration_sec", usage->duration_sec);
            } else {
                cJSON_AddStringToObject(data_obj, "usage_id", TEST_UUID);
                cJSON_AddStringToObject(data_obj, "session_type", "unknown");
                cJSON_AddNumberToObject(data_obj, "cat_weight", 3500.0);
                cJSON_AddNumberToObject(data_obj, "waste_raw", 10.0);
                cJSON_AddNumberToObject(data_obj, "duration_sec", 120);
            }
            break;

        case MESSEGE_CLEAN_RESULT:
            if (mqtt_packet->data != NULL) {
                CLEAN_RESULT_Packet_t* clean = (CLEAN_RESULT_Packet_t*)mqtt_packet->data;
                cJSON_AddStringToObject(data_obj, "clean_id", TEST_UUID);
                cJSON_AddStringToObject(data_obj, "trigger", "device_button");
                cJSON_AddStringToObject(data_obj, "target_usage_ids", "");
                cJSON_AddNumberToObject(data_obj, "disposed", clean->disposed);
                cJSON_AddNumberToObject(data_obj, "waste_ratio", clean->waste_ratio);
                cJSON_AddNumberToObject(data_obj, "waste_type", clean->waste_type);
                cJSON_AddNumberToObject(data_obj, "send_level", clean->send_level);
                cJSON_AddStringToObject(data_obj, "request_token_ref", TEST_UUID);
            } else {
                cJSON_AddStringToObject(data_obj, "clean_id", TEST_UUID);
                cJSON_AddStringToObject(data_obj, "trigger", "device_button");
                cJSON_AddNumberToObject(data_obj, "disposed", 10.0);
                cJSON_AddNumberToObject(data_obj, "waste_ratio", 1.0);
                cJSON_AddNumberToObject(data_obj, "waste_type", 1);
                cJSON_AddNumberToObject(data_obj, "send_level", 0);
            }
            break;

        case MESSEGE_DIAGNOSTICS:
            cJSON_AddStringToObject(data_obj, "alert_level", "critical");
            cJSON_AddStringToObject(data_obj, "fault_code", "LOADCELL_ERR");
            cJSON_AddStringToObject(data_obj, "affected_subsystem", "loadcell");
            sensor_obj = cJSON_CreateObject();
            cJSON_AddItemToObject(data_obj, "context", sensor_obj);
            cJSON_AddNumberToObject(data_obj, "uptime_sec", 1747396800);
            cJSON_AddNumberToObject(data_obj, "reset_reason", esp_reset_reason());
            cJSON_AddNumberToObject(data_obj, "rssi_dbm", 0);
            subsystems = cJSON_CreateObject();
            cJSON_AddItemToObject(data_obj, "subsystems", subsystems); // 오타 정정
            break;

        case MESSEGE_HEALTH:
            cJSON_AddNumberToObject(data_obj, "uptime_sec", esp_log_timestamp() / 1000);
            cJSON_AddNumberToObject(data_obj, "reset_reason", esp_reset_reason());
            cJSON_AddNumberToObject(data_obj, "rssi_dbm", -70);
            subsystems = cJSON_CreateObject();
            cJSON_AddStringToObject(subsystems, "motor", "ok");
            cJSON_AddStringToObject(subsystems, "loadcell", "ok");
            cJSON_AddStringToObject(subsystems, "tof", "ok");
            cJSON_AddStringToObject(subsystems, "ble", "ok");
            cJSON_AddItemToObject(data_obj, "subsystems", subsystems); // 오타 정정

            cJSON_AddStringToObject(data_obj, "power_source", "ADAPTER");
            cJSON_AddNumberToObject(data_obj, "sand_level", 0);
            cJSON_AddNumberToObject(data_obj, "bin_weight", 0);
            cJSON_AddNumberToObject(data_obj, "cover_open", 0);
            cJSON_AddNumberToObject(data_obj, "maintenance_mode", 0);
            cJSON_AddNumberToObject(data_obj, "uv_status", 0);
            cJSON_AddNumberToObject(data_obj, "free_heap", esp_get_free_heap_size());
            break;

        default:
            cJSON_Delete(data_obj);
            return NULL;        
    }
    return data_obj;
}

cJSON* create_t100_subsystems_json(fault_code fault)
{
    cJSON *susbsys_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(susbsys_obj, "power",   (fault.bit.Bat_Status > 1) ? "fault" : "ok");
    cJSON_AddStringToObject(susbsys_obj, "imu",     (fault.bit.IMU_Err)       ? "fault" : "ok");
    cJSON_AddStringToObject(susbsys_obj, "ble",     (fault.bit.BLE_Err)       ? "fault" : "ok");
    cJSON_AddStringToObject(susbsys_obj, "storage", (fault.bit.storage)       ? "fault" : "ok");
    return susbsys_obj;
}

static cJSON* Get_cJSON_Data_for_Tracker(messege_tx_mqtt_cmd_e cmd, tracker_mqtt_packet_t* tracker_mqtt_packet)
{
    cJSON *data_obj = cJSON_CreateObject();
    Motion_Packet_t* packet = &tracker_mqtt_packet->packet;
    const uint16_t * points;
    char dynamicMacStr[30];
    cJSON *susbsys_obj;

    if (data_obj == NULL) return NULL;

    switch (cmd) {
        case TRACKER_MESSEGE_ACTIVITY:
            snprintf(dynamicMacStr, sizeof(dynamicMacStr), "TRACKER_%02X%02X%02X%02X%02X%02X",
            tracker_mqtt_packet->mac[0], tracker_mqtt_packet->mac[1], tracker_mqtt_packet->mac[2],
            tracker_mqtt_packet->mac[3], tracker_mqtt_packet->mac[4], tracker_mqtt_packet->mac[5]);
            cJSON_AddStringToObject(data_obj, "tracker_id", dynamicMacStr);
            cJSON_AddStringToObject(data_obj, "activity_id", TEST_UUID);
            cJSON_AddNumberToObject(data_obj, "seq_num", packet->motion_data.seq);
            cJSON_AddNumberToObject(data_obj, "interval_min", motion_res.motion_req.interval);
            cJSON_AddNumberToObject(data_obj, "total_points", motion_res.motion_req.total_points);
            
            cJSON *points_array = cJSON_CreateArray();
            points = (const uint16_t *)&tracker_mqtt_packet->packet.motion_data.pack_data_0;
            int current_pkt_points = (motion_res.motion_req.total_points < 9) ? motion_res.motion_req.total_points : 9;

            for (int i = 0; i < current_pkt_points; i++) {
                cJSON_AddItemToArray(points_array, cJSON_CreateNumber(points[i]));
            }
            cJSON_AddItemToObject(data_obj, "points", points_array);
            cJSON_AddNumberToObject(data_obj, "battery_pct", health_res.health_data_res.Bat_Level);
            break;

        case TRACKER_MESSEGE_DIAGNOSTICS:
            cJSON_AddNumberToObject(data_obj, "seq_num", 1);
            cJSON_AddNumberToObject(data_obj, "uptime_sec", packet->health_data_res.uptime_sec);
            cJSON_AddNumberToObject(data_obj, "reset_reason", packet->health_data_res.fault_flag.bit.reset_reason);
            cJSON_AddNumberToObject(data_obj, "rssi_dbm", packet->health_data_res.target_rssi);

            susbsys_obj = create_t100_subsystems_json(packet->health_data_res.fault_flag);
            cJSON_AddItemToObject(data_obj, "subsystems", susbsys_obj);
            cJSON_AddStringToObject(data_obj, "mode", "operational");
            
            if (packet->health_data_res.fault_flag.bit.IMU_Err) {
                cJSON_AddStringToObject(data_obj, "alert_level", "normal");
                cJSON_AddStringToObject(data_obj, "fault_code", "IMU_ERR");
                cJSON_AddStringToObject(data_obj, "affected_subsystem", "imu");
            } else if (packet->health_data_res.fault_flag.bit.BLE_Err) {
                cJSON_AddStringToObject(data_obj, "alert_level", "normal");
                cJSON_AddStringToObject(data_obj, "fault_code", "BLE_ERR");
                cJSON_AddStringToObject(data_obj, "affected_subsystem", "ble");
            } else {
                cJSON_AddStringToObject(data_obj, "alert_level", "normal");
                cJSON_AddStringToObject(data_obj, "fault_code", "NONE");
                cJSON_AddStringToObject(data_obj, "affected_subsystem", "none");
            }

            cJSON *context = cJSON_CreateObject();
            cJSON_AddNumberToObject(context, "raw_fault_byte", packet->health_data_res.fault_flag.byte);
            cJSON_AddItemToObject(data_obj, "context", context);
            break;

        case TRACKER_MESSEGE_HEALTH:
            cJSON_AddNumberToObject(data_obj, "seq_num", 2);
            cJSON_AddNumberToObject(data_obj, "uptime_sec", packet->health_data_res.uptime_sec);
            cJSON_AddNumberToObject(data_obj, "reset_reason", packet->health_data_res.fault_flag.bit.reset_reason);
            cJSON_AddNumberToObject(data_obj, "rssi_dbm", packet->health_data_res.target_rssi);

            susbsys_obj = create_t100_subsystems_json(packet->health_data_res.fault_flag);
            cJSON_AddItemToObject(data_obj, "subsystems", susbsys_obj);

            cJSON_AddNumberToObject(data_obj, "battery_pct", packet->health_data_res.Bat_Level);
            char fw_str[16];
            snprintf(fw_str, sizeof(fw_str), "v%u.%u.%u", 
                     packet->health_data_res.major, 
                     packet->health_data_res.minor, 
                     packet->health_data_res.patch);
            cJSON_AddStringToObject(data_obj, "fw_version", fw_str);
            cJSON_AddNumberToObject(data_obj, "gateway_rssi_dbm", packet->health_data_res.target_rssi);
            break;

        default:
            cJSON_Delete(data_obj);
            return NULL;
    }
    return data_obj;
}

void Send_cJSON_Messege(mqtt_packet_t* mqtt_packet)
{
    cJSON* root = Get_cJSON_Header(mqtt_packet->cmd);
    if (root == NULL) return;

    cJSON* data = Get_cJSON_Data(mqtt_packet);
    if (data == NULL) {
        cJSON_Delete(root);
        return;
    }

    uint8_t mac_byte[6];
    char dynamicMacStr[13];
    esp_read_mac(mac_byte, ESP_MAC_WIFI_STA);
    snprintf(dynamicMacStr, sizeof(dynamicMacStr), "%02X%02X%02X%02X%02X%02X",
            mac_byte[0], mac_byte[1], mac_byte[2], mac_byte[3], mac_byte[4], mac_byte[5]);

    cJSON_AddItemToObject(root, "data", data);
    char *payloadBuf = cJSON_PrintUnformatted(root);
    
    if (payloadBuf != NULL) {
        char pub_topic[100];
        const char * pubTopic = pub_topic; 
        switch (mqtt_packet->cmd) {
            case MESSEGE_REGISTRATION: snprintf(pub_topic, sizeof(pub_topic), SERVER_TX_TOPIC_REGISTRATION, dynamicMacStr); break;
            case MESSEGE_BOOT:         snprintf(pub_topic, sizeof(pub_topic), SERVER_TX_TOPIC_BOOT, dynamicMacStr); break;
            case MESSEGE_ACCESS:       snprintf(pub_topic, sizeof(pub_topic), SERVER_TX_TOPIC_ACCESS, dynamicMacStr); break;
            case MESSEGE_USAGE:        snprintf(pub_topic, sizeof(pub_topic), SERVER_TX_TOPIC_USAGE, dynamicMacStr); break;
            case MESSEGE_CLEAN_RESULT: snprintf(pub_topic, sizeof(pub_topic), SERVER_TX_TOPIC_CLEAN_RESULT, dynamicMacStr); break;
            case MESSEGE_DIAGNOSTICS:  snprintf(pub_topic, sizeof(pub_topic), SERVER_TX_TOPIC_DIAGNOSTICS, dynamicMacStr); break;
            case MESSEGE_HEALTH:       snprintf(pub_topic, sizeof(pub_topic), SERVER_TX_TOPIC_HEALTH, dynamicMacStr); break;
            default: pubTopic = NULL; break;
        }                          
        if (pubTopic != NULL) {
            bool pubStatus = PublishToTopic(pubTopic, strlen(pubTopic), payloadBuf, strlen(payloadBuf));
            if (pubStatus) {
                ESP_LOGI(TAG, "퍼블리시 성공!");
                ESP_LOGI(TAG, "보낸 페이로드: %s", payloadBuf);
            }
        }
        cJSON_free(payloadBuf);
    }
    cJSON_Delete(root);
}

void Send_cJSON_Messege_for_tracker(tracker_mqtt_packet_t* tracker_mqtt_packet)
{
    Motion_Packet_t* packet = &tracker_mqtt_packet->packet;
    messege_tx_mqtt_cmd_e cmd = tracker_mqtt_packet->cmd;
    if (cmd == TRACKER_MESSEGE_HEALTH)
        memcpy(&health_res, &tracker_mqtt_packet->packet, sizeof(Motion_Packet_t));
    if (packet->event_code == MOTION_START_RESPONSE) {
        memcpy(&motion_res, &tracker_mqtt_packet->packet, sizeof(Motion_Packet_t));
        return;
    }
    cJSON* root = Get_cJSON_Header(cmd);
    if (root == NULL) return;

    cJSON* data = Get_cJSON_Data_for_Tracker(cmd, tracker_mqtt_packet);
    if (data == NULL) {
        cJSON_Delete(root);
        return;
    }

    cJSON_AddItemToObject(root, "data", data);

    char dynamicMacStr[13];
    snprintf(dynamicMacStr, sizeof(dynamicMacStr), "%02X%02X%02X%02X%02X%02X",
            tracker_mqtt_packet->mac[0], tracker_mqtt_packet->mac[1], tracker_mqtt_packet->mac[2],
            tracker_mqtt_packet->mac[3], tracker_mqtt_packet->mac[4], tracker_mqtt_packet->mac[5]);

    char *payloadBuf = cJSON_PrintUnformatted(root);
    
    if (payloadBuf != NULL) {
        char pub_topic[100];
        const char * pubTopic = pub_topic; 
        switch (cmd) {
            case TRACKER_MESSEGE_ACTIVITY: snprintf(pub_topic, sizeof(pub_topic), TRACKER_RX_TOPIC_ACTIVITY, dynamicMacStr); break;
            case TRACKER_MESSEGE_HEALTH:   snprintf(pub_topic, sizeof(pub_topic), TRACKER_RX_TOPIC_HEALTH, dynamicMacStr); break;
            default: pubTopic = NULL; break;
        }                          
        if (pubTopic != NULL) {
            bool pubStatus = PublishToTopic(pubTopic, strlen(pubTopic), payloadBuf, strlen(payloadBuf));
            if (pubStatus) {
                ESP_LOGI(TAG, "퍼블리시 성공!");
                ESP_LOGI(TAG, "보낸 페이로드: %s", payloadBuf);
            }
        }
        cJSON_free(payloadBuf);
    }
    cJSON_Delete(root);
}