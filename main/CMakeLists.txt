set(SOURCES
		"app_config_flash.c" 
		"app_nvs.c" 
		"dc_motor_control.c" 
		"keyscan.c" 
		"led.c" 
		"loadcell.c" 
		"main.c" 
		"motor.c" 
		"sensor.c" 
		"step_motor_control.c" 
		"ui.c" 
		"uv_led.c" 
		"wifi_rssi_test.c" 
		"ble/ble_parse.c" 
		"ble/ble_rx_task.c" 
		"ble/ble_task.c" 
		"ble/ble_tracker_id.c" 
		"ble/ble_tx_task.c" 
		"wifi/http_ota.c" 
		"wifi/wifi_task.c"
)

idf_component_register(

    SRCS 
        ${SOURCES}
    
    INCLUDE_DIRS
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/ble"
        "${CMAKE_CURRENT_LIST_DIR}/wifi"

    REQUIRES 
        freertos 
        esp_event 
        esp_netif 
        esp_https_ota

    PRIV_REQUIRES 
        bt 
        nvs_flash 
        app_update 
        driver 
        esp_driver_mcpwm 
        esp_adc 
        esp_timer 
        esp_wifi
        esp-tls
)
