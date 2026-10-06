#ifndef __WIFI_TASK_H__
#define __WIFI_TASK_H__

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"

#define WIFI_MAX_VALUE 30

extern EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

extern wifi_ap_record_t *ap_list;

void wifi_list_clear(void); // 전역 선언 복원
void Wifi_Disconnect(void);
void wifi_init(void); 
uint16_t wifi_scan_start(void);
void Wifi_Connect(const char* target_ssid, const char* target_password);

#endif /* __WIFI_TASK_H__ */