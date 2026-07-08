#include "main.h"

#define TOF_DUMP_DEBUG

#define MAX_ITEMS 10

#define ROADCELL_CNT_MAX 			6
#define WEIGHT_VALUE_MAX 			6
#define PHOTO_VALUE_MAX 			4
#define TOF_VALUE_MAX 				5

static const char *TAG = "SENSOR";
static SemaphoreHandle_t sensor_mutex;
static int pt_value=0;

#ifdef TOF_DUMP_DEBUG
static void dump_pt_value(int value)
{
	ESP_LOGI(TAG, "SCP_SPIN_ST : %s", (PT_BIT_SCP_SPIN_ST&value)?"ON":"OFF");
	ESP_LOGI(TAG, "SCP_OUT     : %s", (PT_BIT_SCP_OUT&value)?"ON":"OFF");
	ESP_LOGI(TAG, "WASTE_OPEN  : %s", (PT_BIT_WASTE_OPEN&value)?"ON":"OFF");
	ESP_LOGI(TAG, "MCOVER_OPEN : %s", (PT_BIT_MCOVER_OPEN&value)?"ON":"OFF");
	ESP_LOGI(TAG, "SCP_SPIN_ED : %s", (PT_BIT_SCP_SPIN_ED&value)?"ON":"OFF");
	ESP_LOGI(TAG, "SCP_IN      : %s", (PT_BIT_SCP_IN&value)?"ON":"OFF");
	ESP_LOGI(TAG, "WASTE_CLOSE : %s", (PT_BIT_WASTE_CLOSE&value)?"ON":"OFF");
	ESP_LOGI(TAG, "REED_SW     : %s", (PT_BIT_REED_SW&value)?"ON":"OFF");
	ESP_LOGI(TAG, "MCOVER_CLOSE: %s", (PT_BIT_MCOVER_CLOSE&value)?"ON":"OFF");
	ESP_LOGI(TAG, " ");
}
#endif

int set_pt_status(int value)
{
	// PHOTO SENSOR
    if (xSemaphoreTake(sensor_mutex, portMAX_DELAY) == pdTRUE) 
    {
		pt_value = value;
        xSemaphoreGive(sensor_mutex);
    }
#ifdef TOF_DUMP_DEBUG    
    dump_pt_value(value);
#endif
	return 0;
}

int get_pt_status(void)
{
	return pt_value;
}

// 매크로 및 상수 정의
#define TOF_MIN_VALID      	0x300
#define TOF_MAX_VALID      	0x1F00
#define TOF_DEBOUNCE_TIME   30   // 3초 채터링 방지
#define LOOP_INTERVAL_MS   	100    // 메인 루프 주기 (100ms)

// 상태 정의
typedef enum {
    STATE_NO_APPROACH,
    STATE_APPROACH
} ApproachState;

// 글로벌 변수 (실제 구현 시 구조체나 클래스로 묶는 것을 권장)
ApproachState current_state = STATE_NO_APPROACH;
ApproachState prev_state = STATE_NO_APPROACH;
static int chatter_counter_ms = 0;
static int sensor_enable_f = 0;

static int sensor_enable(int enable)
{
	if(enable)
	{
		sensor_enable_f = 1;
	}
	else
	{
		sensor_enable_f = 0;
	}
	return 0;
}

static bool tof_proc(int tof_value)
{
    mt_message_t msg = {0};
    bool is_in_range = (tof_value >= TOF_MIN_VALID && tof_value <= TOF_MAX_VALID);

	if(!sensor_enable_f)
		return 0;
		
    if (is_in_range) {
        // 유효 범위 내 진입 시 즉시 접근 상태로 전환 및 카운터 초기화
        current_state = STATE_APPROACH;
        chatter_counter_ms = 0;
        if(prev_state == STATE_NO_APPROACH)
        {
            ESP_LOGI(TAG, "STATE_APPROACH");
        	prev_state = STATE_APPROACH;
	    	send_motor_msg(&msg, MT_PAUSE_CMD);
		}
    } else {
        // 유효 범위를 벗어났을 때 (접근 해제 조건)
        if (current_state == STATE_APPROACH) {
            chatter_counter_ms++;
            
            // 3초(3000ms) 동안 유효 범위를 벗어나 있어야 최종 해제 처리
            if (chatter_counter_ms >= TOF_DEBOUNCE_TIME) {
                current_state = STATE_NO_APPROACH;
                prev_state = STATE_NO_APPROACH;
                chatter_counter_ms = 0;
                ESP_LOGI(TAG, "STATE_NO_APPROACH");
                send_motor_msg(&msg, MT_RESTORE_CMD);
            }
        }
    }
    return (current_state == STATE_APPROACH);
}

int get_line(char *buf, int limit, char *ptr)
{
	int i, len;
	char *p;
	p = buf;
	for(i=0;i<limit;i++)
	{
		if(*p++ == '\n')
		{
			break;
		}
	}
	if(i >= limit)
	{
		return -1;
	}
	len = i;
	p = buf;
	for(i=0;i<len;i++)
	{
		*ptr++ = *p++;
	}
	return len+1;	// next
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

	len = strlen(buf);	// total string length including new line
	memset((void *)str, 0, UART_BUF_SIZE);
	idx = get_line(buf, len, str);
	ptr = buf+idx;	// next
	while(idx > 0)
	{
	    int values[MAX_ITEMS] = {0};
	    char status[32];
	    
        if (strncmp(str, "SS: ", 4) == 0) {
            data_ptr = str + 4;
            cnt = strlen(data_ptr);
            if(cnt == 41)
            {
            	// valid data or abandon
                sscanf(data_ptr, "%d %d %d %d %d %d %d", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5], &values[6]);
//                ESP_LOGI(TAG, "rd : %d, %d, %d, %d, %d, %d, %d", values[0], values[1], values[2], values[3], values[4], values[5], values[6]);	// debug
//				if(values[6] < 0x1ff0)
//                	ESP_LOGI(TAG, " %04x", values[6]);	// debug
				loadcell_proc(&values[0]);
                tof_proc(values[6]);
            }
            total_used_chars += idx;
        } else if (strncmp(str, "PT: ", 4) == 0) {
            data_ptr = str + 4;
            cnt = strlen(data_ptr);
            if(cnt == 3)
            {
                sscanf(data_ptr, "%d", &values[0]);
//                ESP_LOGI(TAG, "PT : %04x", values[0]);
                set_pt_status(values[0]);
            }
            total_used_chars += idx;
        } else if (strncmp(str, "ST: OK", 6) == 0) {
            ESP_LOGI(TAG, "sensor start!!");
            sensor_enable(1);
            total_used_chars += idx;
        } else if (strncmp(str, "ST: FAIL", 8) == 0) {
            ESP_LOGI(TAG, "sensor FAILED !!");
            total_used_chars += idx;
        } else if (strncmp(str, "ST: ", 4) == 0) {
            data_ptr = str + 4;
            cnt = strlen(data_ptr);
            sscanf(data_ptr, "%s", &status[0]);
            ESP_LOGI(TAG, "%s", &status[0]);
            total_used_chars += idx;
        } else {
            ESP_LOGI(TAG, "unknown %s", str);
            total_used_chars += idx; // abandon
        }

		len = strlen(ptr);
		if(len <= 0)
		{
			break;
		}
		memset((void *)str, 0, UART_BUF_SIZE);
        idx = get_line(ptr, len, str);
        if(idx <= 0)
        {
        	break;
        }
        ptr += idx;
        
	}
	
	free(str);
	free(buf);
	return total_used_chars;
}

void sensor_init(void)
{
	int cnt;
	sensor_mutex = xSemaphoreCreateMutex();
	cnt = 0;
	do{
        uart_write_bytes(UART_NUM_1, (const char *)"ss\n", strlen("ss\n"));
		cnt++;
		if(cnt > 3)
		{
			ESP_LOGI(TAG, "sensor communication error !!");
		}
        vTaskDelay(pdMS_TO_TICKS(500));
        if(sensor_enable_f)
        {
			break;
        }
	} while(1);
}
