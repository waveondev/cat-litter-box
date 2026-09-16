#include "main.h"

static const char *TAG = "APP_MAIN";

// 핀 설정 (ESP32-S3 하드웨어에 맞게 핀맵 변경)
#define UART1_TXD_PIN (GPIO_NUM_17)
#define UART1_RXD_PIN (GPIO_NUM_18)

#define A_FUNCTION      "a"
#define B_FUNCTION      "b"
#define C_FUNCTION      "c"
#define D_FUNCTION      "d"
#define E_FUNCTION      "e"
#define F_FUNCTION      "f"
#define G_FUNCTION      "g"
#define H_FUNCTION      "h"
#define I_FUNCTION      "i"
#define J_FUNCTION      "j"
#define K_FUNCTION      "k"
#define L_FUNCTION      "l"
#define M_FUNCTION      "m"
#define N_FUNCTION      "n"
#define O_FUNCTION      "o"
#define P_FUNCTION      "p"
#define Q_FUNCTION      "q"
#define R_FUNCTION      "r"
#define S_FUNCTION      "s"
#define T_FUNCTION      "t" // 기구물 미장착 캘리브레이션
#define U_FUNCTION      "u" // 링 구조 장착 캘리브레이션
#define V_FUNCTION      "v" // 상단 판 장착 캘리브레이션
#define Z_FUNCTION      "z" // E-Stop

#define RX_BUF_SIZE         (256)
#define PARSE_BUF_SIZE      (128)
#define UART_MAX_TIMEOUT    (10)

// ring buffer
typedef struct {
    unsigned char buffer[RX_BUF_SIZE];
    unsigned short head;
    unsigned short tail;
} RingBuffer;

RingBuffer rx_ring;
unsigned char rx_byte; 
char parse_buf[PARSE_BUF_SIZE];
unsigned short parse_idx = 0;

static unsigned int reset_reason = 0;

// 긴급 정지 관련 함수 원형 (motor.c에 구현됨)
extern void set_emergency_stop(void);
extern void clear_emergency_stop(void);

void RingBuffer_Push(unsigned char data) {
    unsigned short next = (rx_ring.head + 1) % RX_BUF_SIZE;
    if (next != rx_ring.tail) { 
        rx_ring.buffer[rx_ring.head] = data;
        rx_ring.head = next;
    }
}

int RingBuffer_Pop(unsigned char *data) {
    if (rx_ring.head == rx_ring.tail) {
        return 0; 
    }
    *data = rx_ring.buffer[rx_ring.tail];
    rx_ring.tail = (rx_ring.tail + 1) % RX_BUF_SIZE;
    return 1; 
}

static void Usage(void)
{
    ESP_LOGI(TAG, "%s", __func__);
    ESP_LOGI(TAG, "A : PLATE_CMD FORWARD ");
    ESP_LOGI(TAG, "B : PLATE_CMD REVERSE ");
    ESP_LOGI(TAG, "C : PLATE_CMD STOP ");
    ESP_LOGI(TAG, "D : SCP_INOUT_CMD FORWARD");
    ESP_LOGI(TAG, "E : SCP_INOUT_CMD REVERSE");
    ESP_LOGI(TAG, "F : SCP_INOUT_CMD STOP");
    ESP_LOGI(TAG, "G : WASTE_COVER_CMD FORWARD");
    ESP_LOGI(TAG, "H : WASTE_COVER_CMD REVERSE");
    ESP_LOGI(TAG, "I : WASTE_COVER_CMD STOP");
    ESP_LOGI(TAG, "J : ALL DEMO1 SYNARIO ");
    ESP_LOGI(TAG, "K : SCP_SPIN_CMD FORWARD");
    ESP_LOGI(TAG, "L : SCP_SPIN_CMD REVERSE");
    ESP_LOGI(TAG, "M : SCP_SPIN_CMD STOP");
    ESP_LOGI(TAG, "N : MAIN_COVER_CMD FORWARD");
    ESP_LOGI(TAG, "O : MAIN_COVER_CMD REVERSE");
    ESP_LOGI(TAG, "P : MAIN_COVER_CMD STOP");
    ESP_LOGI(TAG, "Q : LOADCELL CALIBRATION (Tare)");
    ESP_LOGI(TAG, "R : RESET");
    ESP_LOGI(TAG, "S : Motor Calibration");
    ESP_LOGI(TAG, "T : LOADCELL State 1 (No parts) Save");
    ESP_LOGI(TAG, "U : LOADCELL State 2 (Ring) Save");
    ESP_LOGI(TAG, "V : LOADCELL State 3 (Plate) Save");
    ESP_LOGI(TAG, "Z : Emergency Stop");
    ESP_LOGI(TAG, "Enter : ");
}

static void demo_synario(void *arg) {
    ESP_LOGI(TAG, "%s +", __func__);
    do_clean(arg);
    ESP_LOGI(TAG, "%s -", __func__);
    vTaskDelete(NULL);
}

static void loadcell_tare_task(void *arg) {
    ESP_LOGI(TAG, "%s +", __func__);
    vTaskDelay(pdMS_TO_TICKS(5000));
    message_t lmsg = {0};
    send_loadcell_msg(&lmsg, LOADCELL_TARE_CMD);
    ESP_LOGI(TAG, "%s -", __func__);
    vTaskDelete(NULL);
}

static void Process_Command(message_t *mtmsg, char *cmd) 
{
    mt_message_t msg = {0};
    msg.task_id = mtmsg->task_id;

    if (strncmp(cmd, (char *)A_FUNCTION, strlen((char *)A_FUNCTION)) == 0) {
        send_plate_msg(&msg, PLATE_CMD, 0, FORWARD, 0);
    } else if (strncmp(cmd, (char *)B_FUNCTION, strlen((char *)B_FUNCTION)) == 0) {
        send_plate_msg(&msg, PLATE_CMD, 0, REVERSE, 0);
    } else if (strncmp(cmd, (char *)C_FUNCTION, strlen((char *)C_FUNCTION)) == 0) {
        send_plate_msg(&msg, PLATE_CMD, 0, STOP, 0);
    }
    else if (strncmp(cmd, (char *)D_FUNCTION, strlen((char *)D_FUNCTION)) == 0) {
        send_scpinout_msg(&msg, SCP_INOUT_CMD, 30, FORWARD, 10000);
    } else if (strncmp(cmd, (char *)E_FUNCTION, strlen((char *)E_FUNCTION)) == 0) {
        send_scpinout_msg(&msg, SCP_INOUT_CMD, 30, REVERSE, 10000);
    } else if (strncmp(cmd, (char *)F_FUNCTION, strlen((char *)F_FUNCTION)) == 0) {
        send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
    } 
    else if (strncmp(cmd, (char *)G_FUNCTION, strlen((char *)G_FUNCTION)) == 0) {
        send_waste_motor_msg(&msg, WASTE_STEP_CMD, 1000, FORWARD, 15000);
    } else if (strncmp(cmd, (char *)H_FUNCTION, strlen((char *)H_FUNCTION)) == 0) {
        send_waste_motor_msg(&msg, WASTE_STEP_CMD, 1000, REVERSE, 15000);
    } else if (strncmp(cmd, (char *)I_FUNCTION, strlen((char *)I_FUNCTION)) == 0) {
        send_waste_motor_msg(&msg, WASTE_STEP_CMD, 0, STOP, 0);
    }
    else if (strncmp(cmd, (char *)J_FUNCTION, strlen((char *)J_FUNCTION)) == 0) {
        clear_emergency_stop(); 
        xTaskCreate(demo_synario, "demo_synario", 3072, NULL, 10, NULL);
    }
    else if (strncmp(cmd, (char *)K_FUNCTION, strlen((char *)K_FUNCTION)) == 0) {
        send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 120, FORWARD, 15000);
    } else if (strncmp(cmd, (char *)L_FUNCTION, strlen((char *)L_FUNCTION)) == 0) {
        send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 120, REVERSE, 15000);
    } else if (strncmp(cmd, (char *)M_FUNCTION, strlen((char *)M_FUNCTION)) == 0) {
        send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 0, STOP, 0);
    }
    else if (strncmp(cmd, (char *)N_FUNCTION, strlen((char *)N_FUNCTION)) == 0) {
        send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, FORWARD, 10000);
    } else if (strncmp(cmd, (char *)O_FUNCTION, strlen((char *)O_FUNCTION)) == 0) {
        send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, REVERSE, 10000);
    } else if (strncmp(cmd, (char *)P_FUNCTION, strlen((char *)P_FUNCTION)) == 0) {
        send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
    }
    else if (strncmp(cmd, (char *)Q_FUNCTION, strlen((char *)Q_FUNCTION)) == 0) {
        xTaskCreate(loadcell_tare_task, "loadcell_tare_task", 3072, NULL, 10, NULL);
    }
    // [추가] 조립 상태별 캘리브레이션 (T, U, V)
    else if (strncmp(cmd, (char *)T_FUNCTION, strlen((char *)T_FUNCTION)) == 0) {
        ESP_LOGI(TAG, "Command: T - 기구물 미장착 상태 캘리브레이션 시작");
        message_t lmsg = {0};
        send_loadcell_msg(&lmsg, LOADCELL_SAVE_STATE1_CMD);
    } else if (strncmp(cmd, (char *)U_FUNCTION, strlen((char *)U_FUNCTION)) == 0) {
        ESP_LOGI(TAG, "Command: U - 링 구조 장착 상태 캘리브레이션 시작");
        message_t lmsg = {0};
        send_loadcell_msg(&lmsg, LOADCELL_SAVE_STATE2_CMD);
    } else if (strncmp(cmd, (char *)V_FUNCTION, strlen((char *)V_FUNCTION)) == 0) {
        ESP_LOGI(TAG, "Command: V - 링+원구조 판 장착 상태 캘리브레이션 시작");
        message_t lmsg = {0};
        send_loadcell_msg(&lmsg, LOADCELL_SAVE_STATE3_CMD);
    }
    else if (strncmp(cmd, (char *)R_FUNCTION, strlen((char *)R_FUNCTION)) == 0) {
        //system_reset(REASON_TEST); // 단순 재부팅만 됨
        system_reset(REASON_FACTORY_RESTORE); // 진짜 공장 초기화 실행
    }
    // [추가] 긴급 정지
    else if (strncmp(cmd, (char *)Z_FUNCTION, strlen((char *)Z_FUNCTION)) == 0) {
        ESP_LOGI(TAG, "%s : EMERGENCY STOP !!", (char *)Z_FUNCTION);
        set_emergency_stop(); 
    }
}

static int uart_parser(message_t *msg)
{
    unsigned char ch;
    while(RingBuffer_Pop(&ch)) 
    {
        if (ch == '$') { 
            parse_idx = 0;
            return 0;
        }
        if (parse_idx < PARSE_BUF_SIZE - 1) {
            parse_buf[parse_idx++] = (char)ch;
            if (ch == '\r'|| ch == '\n') {
                parse_buf[parse_idx] = '\0'; 
                char cmd_type[16] = {0}; 
                int dir_val = -1;
                int angle_val = 0;
                int parsed_args = sscanf(parse_buf, "%15s %d %d", cmd_type, &dir_val, &angle_val);
                if (parsed_args >= 2) 
                {
                    mt_message_t mt_msg = {0}; 
                    uint32_t target_dir = (dir_val == 1) ? FORWARD : (dir_val == 2) ? REVERSE : STOP;
                    if (strcmp(cmd_type, "MP") == 0) {
                        send_plate_msg(&mt_msg, PLATE_CMD, 0, target_dir, 0);
                    }
                    else if (strcmp(cmd_type, "MI") == 0) {
                        send_scpinout_msg(&mt_msg, SCP_INOUT_CMD, 0, target_dir, 0);
                    }
                    else if (strcmp(cmd_type, "MM") == 0) {
                        send_main_motor_msg(&mt_msg, MAIN_COVER_CMD, 0, target_dir, 0);
                    }
                    else if (strcmp(cmd_type, "MW") == 0) {
                        send_waste_motor_msg(&mt_msg, WASTE_COVER_CMD, 0, target_dir, 0);
                    }
                    else if (strcmp(cmd_type, "MS") == 0) {
                        send_scpspin_motor_msg(&mt_msg, SCP_SPIN_CMD, angle_val, target_dir, 0);
                    }
                    else {
                        Process_Command(msg, parse_buf);
                    }
                }
                else if (strlen(parse_buf) > 0) 
                {
                    Process_Command(msg, parse_buf);  
                }
                parse_idx = 0;               
            }
        } else {
            parse_idx = 0;
        }
    }
    return 0;
}

void uart_bridge_init(void) {
    uart_config_t uart0_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart0_config);
    uart_driver_install(UART_NUM_0, UART_BUF_SIZE * 2, 0, 0, NULL, 0);

    uart_config_t uart1_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_1, &uart1_config);
    uart_set_pin(UART_NUM_1, UART1_TXD_PIN, UART1_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
}

void uart1_to_uart0_task(void *arg) {
    ESP_LOGI(TAG, "%s +", __func__);
    int idx, ret;
    
    unsigned char *data = (unsigned char *) malloc(UART_BUF_SIZE);
    char *buf = (char *) malloc(UART_BUF_SIZE);
    idx = 0;
    memset((void *)buf, 0, UART_BUF_SIZE);
    while (1) {
        int len = uart_read_bytes(UART_NUM_1, data, UART_BUF_SIZE, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            if(idx+len <= UART_BUF_SIZE)
            {
                memcpy((void *)(buf+idx), (void *)data, len);
            }
            else
            {
                idx = 0;
                memset((void *)buf, 0, UART_BUF_SIZE);
                memcpy((void *)(buf+idx), (void *)data, len);
            }
            idx+=len;
            ret = sensor_data_parser(buf);
            if(ret > 0)
            {
                if(idx != ret)
                {
                    memcpy((void *)data, (void *)(buf+ret), idx-ret);
                    memset((void *)buf, 0, UART_BUF_SIZE);
                    memcpy((void *)buf, (void *)data, idx-ret);
                    idx -= ret;
                }
                else
                {
                    memset((void *)buf, 0, UART_BUF_SIZE);
                    idx = 0;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    free(buf);
    free(data);
    vTaskDelete(NULL);
}

void uart_process(void *arg) {
    int i;
    unsigned char *data = (unsigned char *) malloc(UART_BUF_SIZE);
    unsigned char *ptr;
    
    ESP_LOGI(TAG, "%s +", __func__);
    while (1) 
    {
        int len = uart_read_bytes(UART_NUM_0, data, UART_BUF_SIZE, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            ptr = data;
            for(i=0;i<len;i++)
            {
                RingBuffer_Push(*ptr++);
            }
        }
        message_t tx_msg;
        tx_msg.task_id = (uint32_t)arg;
        uart_parser(&tx_msg);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    free(data);
    vTaskDelete(NULL);
}

void check_boot_mode(void)
{
    int mode;
    app_config_t *app = get_app_config();
    mode = app->reset_reason;

    ESP_LOGI(TAG, "%s RESET reason %d ", __func__, app->reset_reason);
    reset_reason |= 1<<mode;

    if(mode != REASON_NORMAL)
    {
        app->reset_reason = REASON_NORMAL;
        app_nvs_save_set();
        vTaskDelay(pdMS_TO_TICKS(500)); 
    }
}

unsigned int get_boot_reason(void)
{
    return reset_reason;
}

void system_reset(int reason)
{
	message_t msg;
	app_config_t *app = get_app_config();
	app->reset_reason = reason;

	if(reason == REASON_DIAG)
	{
        app_nvs_save_set();
        send_led_cmd_msg(&msg, LED_QCQUIT_CMD);
        vTaskDelay(pdMS_TO_TICKS(2000));
	}
	else if(reason == REASON_OTA)
	{
        app_nvs_save_set();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
	}
	else if(reason == REASON_FACTORY_RESTORE)
	{
		app->MIN_VALID_WASTE_RAW = 50;   
		app->CLUMPING_WAIT_MIN = 10; 
        
        // 공장 초기화 시에도 기준 전압의 30% (약 495mA)로 설정
        app->m1_jam_current = 495;
        app->m2_jam_current = 495;
        app->m3_jam_current = 495;
        app->m4_jam_current = 495;
        app->m5_jam_current = 495;
            
		app->CAT_ENTRY_MIN_WEIGHT = 500;
		app->WASTE_TYPE_RATIO_TH = 20;   
		app->EFFECTIVE_DWELL_TIME = 5;   
		app->gate_way_rssi_th = -85;
		app->tof_sense_threshold_l = 250;
		app->tof_sense_threshold_r = 250;
		app->motion_data_time = 1800;
		app_nvs_save_set();
		vTaskDelay(pdMS_TO_TICKS(2000));
	}
	else
	{
        app_nvs_save_set();
		vTaskDelay(pdMS_TO_TICKS(500)); 
	}
	esp_restart();
	while(1)
	{
	    vTaskDelay(pdMS_TO_TICKS(500));
	}
}

unsigned int get_freeheap_size(int line)
{
    return xPortGetFreeHeapSize();
}

#ifdef FEATURE_AWS_IOT
#include "nimble/nimble_port.h"
#include "esp_bt.h"
#define OTA_URL "https://evtago.s3.ap-northeast-2.amazonaws.com/loopoo.bin"
extern void ota_main(const char* URL);
volatile bool g_start_ota_flag = false;

static esp_vfs_spiffs_conf_t spiffs_conf = {
  .base_path = "/spiffs",
  .partition_label = "spiffs_storage",
  .max_files = 5,
  .format_if_mount_failed = true
};

static void filesystem_init(void)
{
    esp_err_t ret = esp_vfs_spiffs_register(&spiffs_conf);
    if (ret != ESP_OK) {
        ESP_LOGE("SPIFFS", "Failed to mount or format filesystem");
        return;
    }
    ESP_LOGI("SPIFFS", "SPIFFS mounted successfully");
}
#endif

void app_main(void) {

    get_freeheap_size((int)__LINE__);

    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    NVS_Flash_init();
    check_boot_mode();
    
#ifdef FEATURE_AWS_IOT
    filesystem_init();
#endif

    vTaskDelay(pdMS_TO_TICKS(100));
    uart_bridge_init();
    xTaskCreate(uart1_to_uart0_task, "u1_to_u0", 3072, NULL, 5, NULL);
    xTaskCreate(uart_process, "u0_to_u1", 3072, NULL, 10, NULL);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "%s Rev_%d:%d_%d_%d-%s/%s ", FW_PRJ_NAME, FW_HW_REV, FW_VER_MAJOR, FW_VER_MINOR, FW_VER_PATCH, FW_VER_TIME, FW_VER_DATE);

    const esp_partition_t *running_part = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "Current Running Partition: %s ", running_part->label);

#ifdef FEATURE_AWS_IOT
    Create_Tracker_Capture_Task();
    ble_task_init();
    wifi_init();
#endif

    led_init();
#ifdef FEATURE_SENSOR_INPUT
    sensor_init();
    loadcell_init();
#endif
    uv_led_init();

    dc_motor_init();
    step_motor_init();
    motor_init();    
    current_monitor_init();
    
    ui_init();
    keyscan_init();

    if(get_keyclean_status())
    {
        diag_init();
        set_status_diag(true);
    }
    else
    {
        if(reset_reason & (1<<REASON_FACTORY_RESTORE))
        {
            message_t msg;
            send_ui_cmd_msg(&msg, UI_PAIRING_CMD);
        }
#ifdef FEATURE_AWS_IOT
        aws_iot_task_init();
        while(1)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            if(get_aws_started()) break;
        }   
        {
            message_t lmsg;
            send_led_cmd_msg(&lmsg, LED_IDLE_CMD);
        }
#endif
    }

    while(1)
    {
        float m_weight, w_weight;
        w_weight = get_weight(LOADCELL_WASTE);
        m_weight = get_weight(LOADCELL_MAIN);
        
        // [주석 해제됨] 무게가 정상적으로 산출되는지 모니터링 (500ms 간격 출력)
        ESP_LOGI(TAG, "main: %.1fg waste: %.1fg", m_weight, w_weight); 
        
        vTaskDelay(pdMS_TO_TICKS(5000));

        if (g_start_ota_flag == true) 
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            g_start_ota_flag = false; 
            if (nimble_port_stop() == 0) {
                nimble_port_deinit();
            }
            esp_bt_controller_disable();
            esp_bt_controller_deinit();
            esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
            vTaskDelay(pdMS_TO_TICKS(1000)); 
            ota_main(OTA_URL);
        }
    }
}
