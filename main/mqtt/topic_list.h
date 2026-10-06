#ifndef __TOPIC_LIST_H__
#define __TOPIC_LIST_H__

#include "device_config.h"

#define TEST_UUID "d8146a41-fcbc-4038-a942-fb2ad64891db"

#define TOPIC_HEADER "things/" CONFIG_DEVICE_TYPE "/" CONFIG_DEVICE_PREFIX

// Server -> Device 수신 (RX) 토픽
#define SERVER_RX_TOPIC_REGISTRATION   TOPIC_HEADER "_%s/result/registration"
#define SERVER_RX_TOPIC_BOOT           TOPIC_HEADER "_%s/result/boot"
#define SERVER_RX_TOPIC_ACCESS         TOPIC_HEADER "_%s/result/access"
#define SERVER_RX_TOPIC_USAGE          TOPIC_HEADER "_%s/result/usage"
#define SERVER_RX_TOPIC_CLEAN_RESULT   TOPIC_HEADER "_%s/result/clean_result"
#define SERVER_RX_TOPIC_DIAGNOSTICS    TOPIC_HEADER "_%s/result/diagnostics"
#define SERVER_RX_TOPIC_HEALTH         TOPIC_HEADER "_%s/result/health"

// Device -> Server 송신 (TX) 토픽
#define SERVER_TX_TOPIC_REGISTRATION   TOPIC_HEADER "_%s/registration"
#define SERVER_TX_TOPIC_BOOT           TOPIC_HEADER "_%s/boot"
#define SERVER_TX_TOPIC_ACCESS         TOPIC_HEADER "_%s/access"
#define SERVER_TX_TOPIC_USAGE          TOPIC_HEADER "_%s/usage"
#define SERVER_TX_TOPIC_CLEAN_RESULT   TOPIC_HEADER "_%s/clean_result"
#define SERVER_TX_TOPIC_DIAGNOSTICS    TOPIC_HEADER "_%s/diagnostics"
#define SERVER_TX_TOPIC_HEALTH         TOPIC_HEADER "_%s/health"

// Tracker 관련 토픽
#define TRACKER_TOPIC_HEADER "things/tracker/TRACKER_"
#define TRACKER_RX_TOPIC_ACTIVITY      TRACKER_TOPIC_HEADER "%s/activity"
#define TRACKER_RX_TOPIC_DIAGNOSTICS   TRACKER_TOPIC_HEADER "%s/diagnostics"
#define TRACKER_RX_TOPIC_HEALTH        TRACKER_TOPIC_HEADER "%s/health"

#endif /* __TOPIC_LIST_H__ */