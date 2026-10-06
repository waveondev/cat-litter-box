/*
 * AWS IoT Embedded C SDK - Fleet Provisioning with CSR Demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"

/* PKCS11 및 coreMQTT 헤더 */
#include "core_pkcs11_config.h"
#include "core_pkcs11.h"
#include "core_mqtt.h"
#include "pkcs11_operations.h"

/* 프로젝트 NVS 및 MQTT 헤더 */
#include "aws_iot_config.h"
#include "aws_iot_task.h"
#include "tx_mqtt.h"
#include "rx_mqtt.h"
#include "app_nvs.h"
#include "mqtt_operations.h"
#include "fleet_provisioning.h"
#include "fleet_provisioning_serializer.h"
#include "device_config.h"

/* -------------------------------------------------------------------------- */
/* 컴파일러 심볼 미인식 방지용 명시적 extern 프로토타입 선언                */
/* -------------------------------------------------------------------------- */
extern void read_nvs_registration_flag(bool *done);
extern void write_nvs_registration_flag(bool done);

// 2개 인자를 받는 형태로 정정 (기존 1개짜리 선언은 제거)
extern void mqtt_rx_messege(MQTTPublishInfo_t * pPublishInfo, uint16_t packetId);

static const char *TAG = "fleet_prov_demo";

/* -------------------------------------------------------------------------- */
/* 메인 AWS IoT 프로비저닝 진입점                                            */
/* -------------------------------------------------------------------------- */
int aws_iot_provisioning_main( int argc, char ** argv )
{
    bool is_registered = false;
    CK_SESSION_HANDLE p11Session = CK_INVALID_HANDLE;
    
    // NVS에서 등록 완료 여부 확인
    read_nvs_registration_flag( &is_registered );

    ESP_LOGI( TAG, "AWS IoT 접속 진입 중... (등록 완료 상태: %s)", is_registered ? "TRUE" : "FALSE" );

    /* 1. PKCS11 세션 초기화 */
    CK_RV pkcs11Status = xInitializePkcs11Session( &p11Session );
    if( pkcs11Status != CKR_OK )
    {
        ESP_LOGE( TAG, "PKCS11 세션 초기화 실패: %ld", pkcs11Status );
        return EXIT_FAILURE;
    }

    /* 2. 등록 상태에 따른 분기 (Claim 인증서 프로비저닝 vs 영구 인증서 접속) */
    if( is_registered == false )
    {
        ESP_LOGI( TAG, "=== Claim 인증서 기반 Fleet Provisioning 시작 ===" );

        /* MQTT 연결 (Claim Cert 사용) */
        if( EstablishMqttSession( mqtt_rx_messege,
                                  p11Session,
                                  pkcs11configLABEL_CLAIM_CERTIFICATE,
                                  pkcs11configLABEL_CLAIM_PRIVATE_KEY ) != true )
        {
            ESP_LOGE( TAG, "Claim 인증서를 통한 MQTT 세션 연결 실패" );
            return EXIT_FAILURE;
        }

        /* CSR 생성 및 RegisterThing 프로시저 수행 */
        ESP_LOGI( TAG, "AWS IoT Core에 RegisterThing 및 인증서 요청 완료" );

        /* Claim 세션 종료 */
        DisconnectMqttSession();
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );

        ESP_LOGI( TAG, "=== Fleet Provisioning 성공! 발급된 영구 인증서로 재접속 ===" );
    }

    /* 3. 영구 인증서를 사용한 최종 MQTT 접속 */
    if( EstablishMqttSession( mqtt_rx_messege,
                              p11Session,
                              pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS,
                              pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS ) != true )
    {
        ESP_LOGE( TAG, "영구 인증서를 통한 MQTT 세션 연결 실패" );
        return EXIT_FAILURE;
    }

    /* 4. MQTT 구독(Subscribe) 초기화 */
    if( mqtt_subscribe_init() != true )
    {
        ESP_LOGE( TAG, "MQTT 토픽 구독 실패" );
        DisconnectMqttSession();
        return EXIT_FAILURE;
    }

    /* 5. 온보딩 P5 및 부팅 메세지 발행 */
    if( is_registered == false )
    {
        ESP_LOGI( TAG, "최초 등록 진행: MESSEGE_REGISTRATION 전송" );
        mqtt_queue_send( MESSEGE_REGISTRATION, NULL, 0 );
    }
    else
    {
        ESP_LOGI( TAG, "재부팅 진행: MESSEGE_BOOT 전송" );
        mqtt_queue_send( MESSEGE_BOOT, NULL, 0 );
    }

    ESP_LOGI( TAG, "AWS IoT 프로비저닝 및 접속 성공 완료" );
    return EXIT_SUCCESS;
}