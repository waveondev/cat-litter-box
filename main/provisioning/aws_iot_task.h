#ifndef __AWS_IOT_TASK_H__
#define __AWS_IOT_TASK_H__

#include "tx_mqtt.h"
#include "ble_parse.h"

void aws_iot_task_init(void);
bool get_aws_started(void);

// 3개의 매개변수를 받도록 프로토타입 수정
bool mqtt_queue_send(messege_tx_mqtt_cmd_e cmd, void* data, uint32_t data_len);

void tracker_mqtt_queue_send(messege_tx_mqtt_cmd_e cmd, uint8_t* mac, Motion_Packet_t* packet, uint32_t data_len, pack_data* data);

#endif /* __AWS_IOT_TASK_H__ */