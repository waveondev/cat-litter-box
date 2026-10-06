#include "main.h"
#include "esp_timer.h"
#include <stdio.h>

static const char *TAG = "SENSOR";

#define MAX_ITEMS 10

#ifdef FEATURE_TOF
#define TOF_MIN_VALID      	0
#define TOF_MAX_VALID      	2000
#define TOF_DEBOUNCE_TIME   30   

// TOF 링버퍼 설정 (최근 10개 패킷 보관)
#define TOF_RING_BUF_SIZE   10
static volatile int g_tof_ring_buf[TOF_RING_BUF_SIZE] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static volatile int g_tof_ring_idx = 0;
static volatile int g_tof_ring_count = 0;

typedef enum {
    STATE_NO_APPROACH,
    STATE_APPROACH
} ApproachState;

ApproachState current_state = STATE_NO_APPROACH;
ApproachState prev_state = STATE_NO_APPROACH;
static int chatter_counter_ms = 0;

static volatile bool tof_enable_f = false;
static volatile int last_tof_value = -1; 

#endif

static bool sensor_enable_f = true;

#define ROADCELL_CNT_MAX 			6
#define WEIGHT_VALUE_MAX 			6
#define PHOTO_VALUE_MAX 			4
#define TOF_VALUE_MAX 				5

static SemaphoreHandle_t sensor_mutex = NULL;
static int pt_value = 0;
static int prev_pt_value = -1;

int set_pt_status(int value)
{
    if (sensor_mutex != NULL) {
        if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            pt_value = value;
            xSemaphoreGive(sensor_mutex);
            return 0;
        }
    }
    pt_value = value; 
    return 0;
}

int get_pt_status(void)
{
	return pt_value;
}

static int set_sensor_enable(bool enable)
{
	sensor_enable_f = enable;
	return 0;
}

bool get_sensor_enable(void)
{
	return sensor_enable_f;
}

int set_tof_sensor_enable(bool enable)
{
#ifdef FEATURE_TOF
    tof_enable_f = enable;
    if (tof_enable_f) {
        current_state = STATE_NO_APPROACH;
        prev_state = STATE_NO_APPROACH;
        chatter_counter_ms = 0;
    }
#endif
    return 0;
}

#ifdef FEATURE_TOF

bool get_tof_sensor_enable(void)
{
	return tof_enable_f;
}

/**
 * @brief TOF 링버퍼 초기화 (Lock-free)
 */
void reset_tof_ring_buffer(void)
{
    for (int i = 0; i < TOF_RING_BUF_SIZE; i++) {
        g_tof_ring_buf[i] = -1;
    }
    g_tof_ring_idx = 0;
    g_tof_ring_count = 0;
    last_tof_value = -1;
    ESP_LOGW(TAG, "[TOF DIAG] >>> RING BUFFER RESET COMPLETED <<<");
}

int get_tof_ring_count(void)
{
    return g_tof_ring_count;
}

/**
 * @brief 링버퍼 최솟값 추출
 */
int get_tof_distance(void)
{
    int min_val = 9999;
    int snapshot[TOF_RING_BUF_SIZE] = {0};
    int current_count = g_tof_ring_count;

    if (current_count > 0) {
        for (int i = 0; i < TOF_RING_BUF_SIZE; i++) {
            snapshot[i] = g_tof_ring_buf[i];
            if (snapshot[i] >= 0 && snapshot[i] <= 2000) {
                if (snapshot[i] < min_val) {
                    min_val = snapshot[i];
                }
            }
        }
    }

    ESP_LOGW(TAG, "[TOF SNAPSHOT] Count: %d | Buf: [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d] | EvaluatedMin: %d",
             current_count,
             snapshot[0], snapshot[1], snapshot[2], snapshot[3], snapshot[4],
             snapshot[5], snapshot[6], snapshot[7], snapshot[8], snapshot[9],
             (current_count > 0 && min_val != 9999) ? min_val : -1);

    if (current_count > 0 && min_val != 9999) {
        return min_val;
    }

    ESP_LOGE(TAG, "[TOF ERROR] No valid TOF data in buffer! Forcing return -1");
    return -1;
}

/**
 * @brief TOF 데이터 수신 처리 (Lock-free 적용으로 뮤텍스 데드락 원천 차단)
 */
static int tof_proc(int tof_value)
{
    int processed_tof = tof_value;

    if (processed_tof == 8191) {
        processed_tof = 2000;
    }

    if (!sensor_enable_f) {
        return -1;
    }

    last_tof_value = processed_tof;
    int idx = g_tof_ring_idx;

    g_tof_ring_buf[idx] = processed_tof;
    
    // 🌟 [로그 제거]: 매 패킷 수신 시마다 콘솔을 도배하던 로그 주석 처리
    // ESP_LOGI(TAG, "[TOF RX PUSH] WriteIdx: %d | RawVal: %d -> Clamped: %d | TotalCount: %d", 
    //          idx, tof_value, processed_tof, g_tof_ring_count + 1);

    g_tof_ring_idx = (idx + 1) % TOF_RING_BUF_SIZE;
    if (g_tof_ring_count < TOF_RING_BUF_SIZE) {
        g_tof_ring_count++;
    }

    if (!tof_enable_f) {
        return -2;
    }

    bool is_in_range = (processed_tof >= TOF_MIN_VALID && processed_tof <= TOF_MAX_VALID);

    if (is_in_range) {
        current_state = STATE_APPROACH;
        chatter_counter_ms = 0;
        if (prev_state == STATE_NO_APPROACH) {
            prev_state = STATE_APPROACH;
            ESP_LOGI(TAG, "STATE_APPROACH (Raw: %d -> Processed: %d)", tof_value, processed_tof);
        }
    } else {
        if (current_state == STATE_APPROACH) {
            chatter_counter_ms++;
            
            if (chatter_counter_ms >= TOF_DEBOUNCE_TIME) {
                current_state = STATE_NO_APPROACH;
                prev_state = STATE_NO_APPROACH;
                chatter_counter_ms = 0;
                ESP_LOGI(TAG, "STATE_NO_APPROACH (Raw: %d -> Processed: %d)", tof_value, processed_tof);
            }
        }
    }
    return 0;
}
#else
int get_tof_distance(void)
{
    return -1;
}
void reset_tof_ring_buffer(void) {}
int get_tof_ring_count(void) { return 0; }
#endif

static int get_line(char *buf, int limit, char *ptr)
{
	int i, len;
	char *p = buf;
	for(i=0;i<limit;i++)
	{
		if(*p == '\n') break;
		p++;
	}
	if(i >= limit) return -1;
	len = i;
	p = buf;
	for(i=0;i<len;i++) *ptr++ = *p++;
	return len+1;	
}

int sensor_data_parser(char *input)
{
	int len, idx, cnt;
    int total_used_chars = 0;
    
    if (input == NULL || strlen(input) == 0) return 0;

    char *buf = strdup(input);
    char *ptr;
    char *str = (char *) malloc(UART_BUF_SIZE);
    char *data_ptr = NULL;
    
    if (buf == NULL) return 0;

	len = strlen(buf);	
	memset((void *)str, 0, UART_BUF_SIZE);
	idx = get_line(buf, len, str);
	ptr = buf+idx;	
	
	while(idx > 0)
	{
	    int values[MAX_ITEMS] = {0};
	    char status[32];
	    
        if (strncmp(str, "SS: ", 4) == 0) {
            data_ptr = str + 4;
            cnt = strlen(data_ptr);
            if(cnt < UART_BUF_SIZE-4)
            {
                sscanf(data_ptr, "%d %d %d %d %d %d %d %d", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5], &values[6], &values[7]);
				loadcell_proc(&values[0]);

#ifdef FEATURE_TOF
                // 🌟 [로그 제거]: 매 패킷 수신 시마다 콘솔을 도배하던 로그 주석 처리
                // ESP_LOGI(TAG, "[PARSER SS:] Parsed TOF Raw Val (values[6]): %d", values[6]);
                tof_proc(values[6]);	
#endif

                int current_pt = values[7] & 0xFFFF;
                int scp_spin_mask = (PT_BIT_SCP_SPIN_ST | PT_BIT_SCP_SPIN_ED);
                current_pt ^= scp_spin_mask;

                set_pt_status(current_pt);

                if (prev_pt_value != -1 && current_pt != prev_pt_value) {
                    ESP_LOGI(TAG, "=================================================");
                    ESP_LOGI(TAG, "[SENSOR VALUE CHANGED] 0x%04X (%d) -> 0x%04X (%d)", 
                             prev_pt_value, prev_pt_value, current_pt, current_pt);

                    struct {
                        int mask;
                        const char* name;
                    } sensor_list[] = {
                        { PT_BIT_SCP_SPIN_ST, "SCP_SPIN_ST (스쿱 회전 시작)" },
                        { PT_BIT_SCP_OUT,     "SCP_OUT     (스쿱 전진 끝)" },
                        { PT_BIT_WASTE_CLOSE, "WASTE_CLOSE (배변통 닫힘)" },
                        { PT_BIT_MCOVER_OPEN, "MCOVER_OPEN (메인커버 열림)" },
                        { PT_BIT_SCP_SPIN_ED, "SCP_SPIN_ED (스쿱 회전 끝)" },
                        { PT_BIT_SCP_IN,      "SCP_IN      (스쿱 후진 끝)" },
                        { PT_BIT_WASTE_OPEN,  "WASTE_OPEN  (배변통 열림)" },
                        { PT_BIT_REED_SW,     "REED_SW     (리드 스위치)" },
                        { PT_BIT_MCOVER_CLOSE,"MCOVER_CLOSE(메인커버 닫힘)" }
                    };

                    int list_size = sizeof(sensor_list) / sizeof(sensor_list[0]);
                    for (int i = 0; i < list_size; i++) {
                        int old_state = (prev_pt_value & sensor_list[i].mask) ? 1 : 0;
                        int new_state = (current_pt & sensor_list[i].mask) ? 1 : 0;

                        if (old_state != new_state) {
                            ESP_LOGI(TAG, "  └─ %-26s : %d -> %d (%s)", 
                                     sensor_list[i].name, 
                                     old_state, 
                                     new_state, 
                                     new_state ? "DETECTED/HIGH" : "RELEASED/LOW");
                        }
                    }
                    ESP_LOGI(TAG, "=================================================");
                }
                prev_pt_value = current_pt;
            }
            total_used_chars += idx;
        } else if (strncmp(str, "ST: OK", 6) == 0) {
            ESP_LOGI(TAG, "sensor start!!");
            set_sensor_enable(true);
            total_used_chars += idx;
        } else if (strncmp(str, "ST: FAIL", 8) == 0) {
            ESP_LOGI(TAG, "sensor FAILED !!");
            total_used_chars += idx;
        } else if (strncmp(str, "ER: ", 4) == 0) {
            data_ptr = str + 4;
            cnt = strlen(data_ptr);
            if(cnt == 3) sscanf(data_ptr, "%d", &values[0]);
            ESP_LOGI(TAG, "sensor board error : %d", values[0]);
            message_t lmsg = {0};
            send_led_cmd_msg(&lmsg, LED_ERROR_CMD);
            total_used_chars += idx;
        } else if (strncmp(str, "ST: ", 4) == 0) {
            data_ptr = str + 4;
            cnt = strlen(data_ptr);
            sscanf(data_ptr, "%s", &status[0]);
            ESP_LOGI(TAG, "%s", &status[0]);
            total_used_chars += idx;
        } else {
            ESP_LOGI(TAG, ">> %s", str);
            total_used_chars += idx; 
        }

		len = strlen(ptr);
		if(len <= 0) break;
		memset((void *)str, 0, UART_BUF_SIZE);
        idx = get_line(ptr, len, str);
        if(idx <= 0) break;
        ptr += idx;
	}
	
	free(str);
	free(buf);
	return total_used_chars;
}

void sensor_init(void)
{
	int cnt = 0;
    message_t lmsg = {0};
    
	sensor_mutex = xSemaphoreCreateMutex();
    
    sensor_enable_f = true;

	do{
        uart_write_bytes(UART_NUM_1, (const char *)"ss\n", strlen("ss\n"));
		cnt++;
		if(cnt > 3)
		{
			if(cnt == 4) send_led_cmd_msg(&lmsg, LED_ERROR_CMD);
			else ESP_LOGI(TAG, "sensor communication error !!");
            break;
		}
        vTaskDelay(pdMS_TO_TICKS(500));
        if(sensor_enable_f) break;
	} while(1);
	uart_write_bytes(UART_NUM_1, (const char *)"$$$$$$$", strlen("$$$$$$$"));
}