idf_component_register(
    SRCS "main.c" "dc_motor_control.c" "step_motor_control.c" "keyscan.c" "uv_led.c" "led.c" "wifi_rssi_test.c" "sensor.c" "ui.c" "motor.c" "loadcell.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver esp_driver_mcpwm esp_adc esp_timer esp_wifi esp_event nvs_flash app_update
)
