#ifndef __TOPIC_LIST_H__
#define __TOPIC_LIST_H__



#define TEST_UUID		"d8146a41-fcbc-4038-a942-fb2ad64891db"				// vincent



#define TOPIC_HEADER "things/c100/C100_"
#define SERVER_RX_TOPIC_REGISTRATION   TOPIC_HEADER "%s/result/registration"
#define SERVER_RX_TOPIC_BOOT           TOPIC_HEADER "%s/result/boot"
#define SERVER_RX_TOPIC_ACCESS         TOPIC_HEADER "%s/result/access"
#define SERVER_RX_TOPIC_USAGE          TOPIC_HEADER "%s/result/usage"
#define SERVER_RX_TOPIC_CLEAN_RESULT   TOPIC_HEADER "%s/result/clean_result"
#define SERVER_RX_TOPIC_DIAGNOSTICS    TOPIC_HEADER "%s/result/diagnostics"
#define SERVER_RX_TOPIC_HEALTH         TOPIC_HEADER "%s/result/health"

#define SERVER_TX_TOPIC_REGISTRATION   TOPIC_HEADER "%s/registration"
#define SERVER_TX_TOPIC_BOOT           TOPIC_HEADER "%s/boot"
#define SERVER_TX_TOPIC_ACCESS         TOPIC_HEADER "%s/access"
#define SERVER_TX_TOPIC_USAGE          TOPIC_HEADER "%s/usage"
#define SERVER_TX_TOPIC_CLEAN_RESULT   TOPIC_HEADER "%s/clean_result"
#define SERVER_TX_TOPIC_DIAGNOSTICS    TOPIC_HEADER "%s/diagnostics"
#define SERVER_TX_TOPIC_HEALTH         TOPIC_HEADER "%s/health"


#define TRACKER_TOPIC_HEADER "things/tracker/TRACKER_"
#define TRACKER_RX_TOPIC_ACTIVITY      TRACKER_TOPIC_HEADER "%s/activity"
#define TRACKER_RX_TOPIC_DIAGNOSTICS   TRACKER_TOPIC_HEADER "%s/diagnostics"

#define TRACKER_RX_TOPIC_HEALTH        TRACKER_TOPIC_HEADER "%s/health"





#endif
