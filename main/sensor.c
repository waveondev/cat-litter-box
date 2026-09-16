#include "main.h"

static const char *TAG = "SENSOR";

#define MAX_ITEMS 10

#ifdef FEATURE_TOF
#define TOF_MIN_VALID      	0x300
#define TOF_MAX_VALID      	0x1F00
#define TOF_DEBOUNCE_TIME   30   // 3초 채터링 방지

typedef enum {
    STATE_NO_APPROACH,
    STATE_APPROACH
} ApproachState;

ApproachState current_state = STATE_NO_APPROACH;
ApproachState prev_state = STATE_NO_APPROACH;
static int chatter_counter_ms = 0;
static SemaphoreHandle_t tof_mutex = NULL;
static bool tof_enable_f = false;

#endif

static bool sensor_enable_f = false;

#define ROADCELL_CNT_MAX 			6
#define WEIGHT_VALUE_MAX 			6
#define PHOTO_VALUE_MAX 			4
#define TOF_VALUE_MAX 				5

static SemaphoreHandle_t sensor_mutex = NULL;

static int pt_value=0;

int set_pt_status(int value)
{
	if(!sensor_enable_f)
	{
		return -1;
	}
	if(sensor_mutex == NULL)
	{
		return -2;
	}
    if (xSemaphoreTake(sensor_mutex, portMAX_DELAY) == pdTRUE) 
    {
		pt_value = value;
        xSemaphoreGive(sensor_mutex);
    }
	return 0;
}

int get_pt_status(void)
{
	return pt_value;
}

static int set_sensor_enable(bool enable)
{
	if(enable)
	{
		sensor_enable_f = true;
	}
	else
	{
		sensor_enable_f = false;
	}
	return 0;
}

bool get_sensor_enable(void)
{
	if(sensor_enable_f)
	{
		return true;
	}
	return false;
}

int set_tof_sensor_enable(bool enable)
{
#ifdef FEATURE_TOF
	if(tof_mutex == NULL)
	{
		return -1;
	}
    if (xSemaphoreTake(tof_mutex, portMAX_DELAY) == pdTRUE) 
    {
        tof_enable_f = enable;
        if(tof_enable_f)
        {
            current_state = STATE_NO_APPROACH;
            prev_state = STATE_NO_APPROACH;
            chatter_counter_ms = 0;
        }
        xSemaphoreGive(tof_mutex);
    }
#endif
    return 0;
}

#ifdef FEATURE_TOF

bool get_tof_sensor_enable(void)
{
	return tof_enable_f;
}

static int tof_proc(int tof_value)
{
    mt_message_t msg = {0};
//    mt_message_t msg = {0};
    bool is_in_range = (tof_value >= TOF_MIN_VALID && tof_value <= TOF_MAX_VALID);

	if(!sensor_enable_f)
	{
		return -1;
	}
	
	if(!tof_enable_f)
	{
		return -2;
	}
		
    if (is_in_range) {
        // 유효 범위 내 진입 시 즉시 접근 상태로 전환 및 카운터 초기화
        current_state = STATE_APPROACH;
        chatter_counter_ms = 0;
        if(prev_state == STATE_NO_APPROACH)
        {
        	prev_state = STATE_APPROACH;
            ESP_LOGI(TAG, "STATE_APPROACH");
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
            }
        }
    }
    return 0;
}
#endif

static int get_line(char *buf, int limit, char *ptr)
{
	int i, len;
	char *p;
	p = buf;
	for(i=0;i<limit;i++)
	{
		if(*p == '\n')
		{
			break;
		}
		p++;
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
            if(cnt < UART_BUF_SIZE-4)
            {
            	// valid data or abandon
                sscanf(data_ptr, "%d %d %d %d %d %d %d %d", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5], &values[6], &values[7]);
					ESP_LOGI(TAG, "M %06d %06d %06d %06d W %06d %06d TOF %03d PT %03d"
						, values[2], values[3], values[4], values[5], values[0], values[1], values[6], values[7]);	// debug
				loadcell_proc(&values[0]);
#ifdef FEATURE_TOF
                tof_proc(values[6]);	// tof
#endif
                set_pt_status(values[7]); // pt
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
            if(cnt == 3)
            {
                sscanf(data_ptr, "%d", &values[0]);
        	}
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
    message_t lmsg = {0};
    
	sensor_mutex = xSemaphoreCreateMutex();
#ifdef FEATURE_TOF
    tof_mutex = xSemaphoreCreateMutex();
#endif
	cnt = 0;
	do{
        uart_write_bytes(UART_NUM_1, (const char *)"ss\n", strlen("ss\n"));
//		uart_write_bytes(UART_NUM_1, (const char *)"ss\n", 3);
		cnt++;
		if(cnt > 3)
		{
			if(cnt == 4)
			{
                send_led_cmd_msg(&lmsg, LED_ERROR_CMD);
			}
			else
			{
				ESP_LOGI(TAG, "sensor communication error !!");
			}
		}
        vTaskDelay(pdMS_TO_TICKS(500));
        if(sensor_enable_f)
        {
			break;
        }
	} while(1);
	uart_write_bytes(UART_NUM_1, (const char *)"$$$$$$$", strlen("$$$$$$$"));
}
