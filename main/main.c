#include "main.h"

#include <stddef.h>
#include <stdint.h>

#include "ble_task.h"
#include "wifi_task.h"
#include "app_config_flash.h"

static const char *TAG = "APP_MAIN";

// 핀 정의 (ESP32-S3 하드웨어에 맞게 수정 가능)
#define UART1_TXD_PIN (GPIO_NUM_17)
#define UART1_RXD_PIN (GPIO_NUM_18)

#define A_FUNCTION 		"a"
#define B_FUNCTION 		"b"
#define C_FUNCTION 		"c"
#define D_FUNCTION 		"d"
#define E_FUNCTION 		"e"
#define F_FUNCTION 		"f"
#define G_FUNCTION 		"g"
#define H_FUNCTION 		"h"
#define I_FUNCTION 		"i"
#define J_FUNCTION 		"j"
#define K_FUNCTION 		"k"
#define L_FUNCTION 		"l"
#define M_FUNCTION 		"m"
#define N_FUNCTION 		"n"
#define O_FUNCTION 		"o"
#define P_FUNCTION 		"p"

#define RX_BUF_SIZE 		(256)
#define PARSE_BUF_SIZE 		(128)
#define UART_MAX_TIMEOUT 	(10)

// ring buffer
typedef struct {
    unsigned char buffer[RX_BUF_SIZE];
    unsigned short head;
    unsigned short tail;
} RingBuffer;

RingBuffer rx_ring;
unsigned char rx_byte; // 1바이트 수신 인터럽트용 변수
char parse_buf[PARSE_BUF_SIZE];
unsigned short parse_idx = 0;

void RingBuffer_Push(unsigned char data) {
    unsigned short next = (rx_ring.head + 1) % RX_BUF_SIZE;
    if (next != rx_ring.tail) { // 버퍼가 가득 차지 않은 경우에만 삽입
        rx_ring.buffer[rx_ring.head] = data;
        rx_ring.head = next;
    }
}

int RingBuffer_Pop(unsigned char *data) {
    if (rx_ring.head == rx_ring.tail) {
        return 0; // 버퍼가 비어있음
    }
    *data = rx_ring.buffer[rx_ring.tail];
    rx_ring.tail = (rx_ring.tail + 1) % RX_BUF_SIZE;
    return 1; // 데이터 추출 성공
}

static void Usage(void)
{
	ESP_LOGI(TAG, "%s", __func__);
	ESP_LOGI(TAG, "A : PLATE_FORWARD_CMD ");
	ESP_LOGI(TAG, "B : PLATE_REVERSE_CMD ");
	ESP_LOGI(TAG, "C : PLATE_STOP_CMD ");
	ESP_LOGI(TAG, "D : SCP_INOUT_FORWARD_CMD ");
	ESP_LOGI(TAG, "E : SCP_INOUT_REVERSE_CMD ");
	ESP_LOGI(TAG, "F : SCP_INOUT_STOP_CMD ");
	ESP_LOGI(TAG, "G : WASTE_FORWARD_CMD ");
	ESP_LOGI(TAG, "H : WASTE_REVERSE_CMD ");
	ESP_LOGI(TAG, "I : WASTE_STOP_CMD ");
	ESP_LOGI(TAG, "J : ALL DEMO1 SYNARIO ");
	ESP_LOGI(TAG, "K : SCP_SPIN_FORWARD_CMD");
	ESP_LOGI(TAG, "L : SCP_SPIN_REVERSE_CMD");
	ESP_LOGI(TAG, "M : SCP_SPIN_STOP_CMD");
	ESP_LOGI(TAG, "N : MAIN_FORWARD_CMD");
	ESP_LOGI(TAG, "O : MAIN_REVERSE_CMD");
	ESP_LOGI(TAG, "P : MAIN_STOP_CMD");
	ESP_LOGI(TAG, "Enter : ");
}

static void demo_synario(void *arg) {
    ESP_LOGI(TAG, "%s +", __func__);

#ifdef FEATURE_LED_TEST
	int i;
    message_t msg;
    msg.task_id = (uint32_t)arg;

	for(i=0;i<LED_CMD_MAX;i++)
	{
		msg.task_id = (uint32_t)arg;
		send_led_cmd_msg(&msg, i);
	    vTaskDelay(pdMS_TO_TICKS(5000));
   }
#endif
	scp_spin_cal(arg);

    ESP_LOGI(TAG, "%s -", __func__);
    vTaskDelete(NULL);
}

static void Process_Command(message_t *mtmsg, char *cmd) 
{
	mt_message_t msg = {0};
	msg.task_id = mtmsg->task_id;
    if (strncmp(cmd, (char *)A_FUNCTION, strlen((char *)A_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)A_FUNCTION);
    	send_dc_motor_msg(&msg, PLATE_FORWARD_CMD);
    } 
    else if (strncmp(cmd, (char *)B_FUNCTION, strlen((char *)B_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)B_FUNCTION);
    	send_dc_motor_msg(&msg, PLATE_REVERSE_CMD);
    } 
    else if (strncmp(cmd, (char *)C_FUNCTION, strlen((char *)C_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)C_FUNCTION);
    	send_dc_motor_msg(&msg, PLATE_STOP_CMD);
    } 
    else if (strncmp(cmd, (char *)D_FUNCTION, strlen((char *)D_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)D_FUNCTION);
    	send_dc_motor_msg(&msg, SCP_INOUT_FORWARD_CMD);
    } 
    else if (strncmp(cmd, (char *)E_FUNCTION, strlen((char *)E_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)E_FUNCTION);
    	send_dc_motor_msg(&msg, SCP_INOUT_REVERSE_CMD);
    } 
    else if (strncmp(cmd, (char *)F_FUNCTION, strlen((char *)F_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)F_FUNCTION);
    	send_dc_motor_msg(&msg, SCP_INOUT_STOP_CMD);
    } 
    else if (strncmp(cmd, (char *)G_FUNCTION, strlen((char *)G_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)G_FUNCTION);
    	send_step_motor_msg(&msg, WASTE_FORWARD_CMD);
    } 
    else if (strncmp(cmd, (char *)H_FUNCTION, strlen((char *)H_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)H_FUNCTION);
    	send_step_motor_msg(&msg, WASTE_REVERSE_CMD);
    } 
    else if (strncmp(cmd, (char *)I_FUNCTION, strlen((char *)I_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)I_FUNCTION);
    	send_step_motor_msg(&msg, WASTE_STOP_CMD);
    }
    else if (strncmp(cmd, (char *)J_FUNCTION, strlen((char *)J_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)J_FUNCTION);
    	xTaskCreate(demo_synario, "demo_synario", 3072, NULL, 10, NULL);
    }
    else if (strncmp(cmd, (char *)K_FUNCTION, strlen((char *)K_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)K_FUNCTION);
    	msg.angle = 90;
    	send_step_motor_msg(&msg, SCP_SPIN_FORWARD_CMD);
    }
    else if (strncmp(cmd, (char *)L_FUNCTION, strlen((char *)L_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)L_FUNCTION);
    	msg.angle = 90;
    	send_step_motor_msg(&msg, SCP_SPIN_REVERSE_CMD);
    }
    else if (strncmp(cmd, (char *)M_FUNCTION, strlen((char *)M_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)M_FUNCTION);
    	msg.angle = 0;
    	send_step_motor_msg(&msg, SCP_SPIN_STOP_CMD);
    }
    else if (strncmp(cmd, (char *)N_FUNCTION, strlen((char *)N_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)N_FUNCTION);
    	send_step_motor_msg(&msg, MAIN_FORWARD_CMD);
    }
    else if (strncmp(cmd, (char *)O_FUNCTION, strlen((char *)O_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)O_FUNCTION);
    	send_step_motor_msg(&msg, MAIN_REVERSE_CMD);
    }
    else if (strncmp(cmd, (char *)P_FUNCTION, strlen((char *)P_FUNCTION)) == 0) 
    {
    	ESP_LOGI(TAG, "%s", (char *)P_FUNCTION);
    	send_step_motor_msg(&msg, MAIN_STOP_CMD);
    }
    else 
    {
    	ESP_LOGI(TAG, "%s : Unknown command !!", __func__);
    }
}

static int uart_parser(message_t *msg)
{
    unsigned char ch;
    
    while(RingBuffer_Pop(&ch)) 
    {
        if (ch == '$') { // 시작 문자 감지 시 버퍼 초기화
            parse_idx = 0;
            return 0;
        }
        
        if (parse_idx < PARSE_BUF_SIZE - 1) {
            parse_buf[parse_idx++] = (char)ch;
            
            // 종료 문자 '\n' 감지 시 명령어 처리
            if (ch == '\r'|| ch == '\n') {
                parse_buf[parse_idx] = '\0'; // 문자열 종결 처리
                Process_Command(msg, parse_buf);  // 처리 함수 호출
                parse_idx = 0;               // 파서 버퍼 초기화
            }
        } else {
            // 버퍼 오버플로우 방지를 위한 초기화
            parse_idx = 0;
        }
    }
    return 0;
}

void uart_bridge_init(void) {
    // 1. UART0 설정 (기본 핀 사용)
    uart_config_t uart0_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart0_config);
    uart_driver_install(UART_NUM_0, UART_BUF_SIZE * 2, 0, 0, NULL, 0);

    // 2. UART1 설정 및 핀 지정
    uart_config_t uart1_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_1, &uart1_config);
    // UART1 전용 GPIO 핀 바인딩
    uart_set_pin(UART_NUM_1, UART1_TXD_PIN, UART1_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
}

// UART1 -> UART0 전달 태스크
void uart1_to_uart0_task(void *arg) {
    ESP_LOGI(TAG, "%s +", __func__);
	int idx, ret;
	
    unsigned char *data = (unsigned char *) malloc(UART_BUF_SIZE);
    char *buf = (char *) malloc(UART_BUF_SIZE);
    idx = 0;
    memset((void *)buf, 0, UART_BUF_SIZE);
    while (1) {
#if 0
        message_t msg;
        if (xQueueReceive(dc_motor_msg, &msg, portMAX_DELAY) == pdPASS) {
        	ESP_LOGI(TAG, "[Receiver %ld] transfer complete -> cmd %d ", msg.task_id, msg.cmd);
        }
#else
        // UART1 데이터 읽기 (최대 20ms 대기)
        int len = uart_read_bytes(UART_NUM_1, data, UART_BUF_SIZE, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
//            uart_write_bytes(UART_NUM_0, (const char *) data, len);
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
#endif        
    }
    free(buf);
    free(data);
//    ESP_LOGI(TAG, "%s -", __func__);
    vTaskDelete(NULL);
}

// UART0 -> UART1 전달 태스크
void uart0_to_uart1_task(void *arg) {
	int i;
    unsigned char *data = (unsigned char *) malloc(UART_BUF_SIZE);
    unsigned char *ptr;
    
    ESP_LOGI(TAG, "%s +", __func__);
    while (1) {
        // UART0 데이터 읽기 (최대 20ms 대기)
        int len = uart_read_bytes(UART_NUM_0, data, UART_BUF_SIZE, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            // 읽은 데이터를 그대로 UART1로 전송
//            uart_write_bytes(UART_NUM_1, (const char *) data, len);
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


void app_main(void) {

    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    NVS_Flash_init();

    uart_bridge_init();
    xTaskCreate(uart1_to_uart0_task, "u1_to_u0", 3072, NULL, 10, NULL);
    xTaskCreate(uart0_to_uart1_task, "u0_to_u1", 3072, NULL, 10, NULL);
    
	ESP_LOGI(TAG, "%s Rev_%d:%d_%d_%d-%s/%s ", FW_PRJ_NAME, FW_HW_REV, FW_VER_MAJOR, FW_VER_MINOR, FW_VER_PATCH, FW_VER_TIME, FW_VER_DATE);

    const esp_partition_t *running_part = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "===============================================");
    ESP_LOGI(TAG, "Current Running Partition: %s", running_part->label);
    ESP_LOGI(TAG, "Application Version: Factory_v1.0.0");
    ESP_LOGI(TAG, "===============================================");

	dc_motor_init();
    step_motor_init();
    motor_task_init();
	keyscan_init();
	uv_led_init();
	led_init();
	ui_init();
#ifdef FEATURE_WIFI_RSSI_TEST
	wifi_test_init();
#endif
#ifdef ENABLE_SENSOR_INPUT
	sensor_init();
	loadcell_init();
#endif
    ble_task_init();
	wifi_init();

	Usage();

	while(1)
	{
		// idle
		vTaskDelay(pdMS_TO_TICKS(1000));
	}	
}
