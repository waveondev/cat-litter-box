#ifndef __BLE_TASK_H__
#define __BLE_TASK_H__

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "app_config_flash.h"
#include "ble_parse.h"
#include "ble_tracker_id.h"

// 큐로 주고받을 데이터 구조체
typedef struct {
    uint16_t len;
    uint8_t data[128]; 
} ble_data_msg_t;
/* Attributes State Machine */

bool ble_send_data_to_queue(const uint8_t *data, uint16_t len);
void ble_task_init(void);
void motion_msg_send(uint8_t cmd,uint8_t sub_cmd);


#endif
