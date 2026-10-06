#ifndef __RX_MQTT_H__
#define __RX_MQTT_H__

#include <stdbool.h>
#include "core_mqtt.h"

bool mqtt_subscribe_init(void);

// 2번째 인자 (uint16_t packetId) 추가
void mqtt_rx_messege(MQTTPublishInfo_t * pPublishInfo, uint16_t packetId);

#endif /* __RX_MQTT_H__ */