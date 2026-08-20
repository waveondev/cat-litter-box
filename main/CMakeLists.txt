# 1. main ���� �ٷ� �Ʒ���, ���� ����(ble, wifi, mqtt, provisioning ��) ���� ��� .c ������ �ڵ����� �ܾ�����ϴ�.
file(GLOB_RECURSE SOURCES "*.c")

if("${target}" STREQUAL "linux")
    set(requires esp_event esp_stubs protocol_examples_common nvs_flash)
endif()


# ������Ʈ ���
idf_component_register(
    SRCS 
        ${SOURCES} # �̷��� �ϸ� ���� �߰� �Ǵ� ������ �ڵ� �ν� �ȴٳ׿�.
    
    INCLUDE_DIRS
        "."
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/ble"
        "${CMAKE_CURRENT_LIST_DIR}/wifi"
        "${CMAKE_CURRENT_LIST_DIR}/mqtt"
        "${CMAKE_CURRENT_LIST_DIR}/provisioning"
        "${CMAKE_CURRENT_LIST_DIR}/../components/common/posix_compat"
        "${CMAKE_CURRENT_LIST_DIR}/config"
        "${CMAKE_CURRENT_LIST_DIR}/crypto"

    # ��������, OTA, NVS flash�� ���� 'driver'(GPIO��)���� �� �� �����մϴ�.
    # ?? esp_https_ota.h ������ �ذ��ϱ� ���� ���⿡ ���������� ����մϴ�.
    REQUIRES 
        freertos 
        esp_event 
        esp_netif 
        protocol_examples_common
        esp_https_ota
        spiffs

    PRIV_REQUIRES 
        bt 
        esp_https_ota 
        nvs_flash 
        app_update 
        bootloader_support 
        driver 
        esp_driver_gpio 
        vfs 
        esp_driver_ledc 
        led_strip 
        esp_adc 
        esp_timer
        json
        coreMQTT                                     
        coreJSON
        corePKCS11
        Fleet-Provisioning-for-AWS-IoT-embedded-sdk
        espressif__cbor
        backoffAlgorithm
        esp_netif
        esp_driver_mcpwm
        wifi_provisioning
)

# target_compile_definitions(${COMPONENT_LIB} PRIVATE
#     "-DconfigGENERATE_RUN_TIME_STATS=1"
#     "-DportGET_RUN_TIME_COUNTER_VALUE()=((unsigned_long)esp_timer_get_time())"
# )