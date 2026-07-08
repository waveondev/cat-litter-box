#include "main.h"

static const char *TAG = "LED";

// 핀 및 LED 개수 설정
#define LED_STRIP_BLINK_GPIO  1// 제어에 사용할 GPIO 핀 번호
#define LED_STRIP_MAX_LEDS    4    // 직렬 연결된 LED 개수

static led_strip_handle_t led_strip;
static SemaphoreHandle_t led_mutex;
static QueueHandle_t led_cmd_msg = NULL;


void send_led_cmd_msg(void *message, uint32_t cmd)
{
	message_t *msg = (message_t *)message;
    msg->cmd = cmd;
    BaseType_t status = xQueueSend(led_cmd_msg, msg, pdMS_TO_TICKS(100));
    if (status == pdPASS) 
    {
//        ESP_LOGI(TAG, "[Sender %ld] transfer complete -> cmd %d ", msg->task_id, msg->cmd);
    } 
    else 
    {
//        ESP_LOGW(TAG, "[Sender %ld] queuse full transfer failed", msg->task_id);
    }
}

static int led_full_rgbw(int mode)
{
	uint32_t color[4];
	
	memset((void *)&color[0], 0, sizeof(color));
	switch(mode)
	{
		case 0:	// r
			color[0] = 255;
			break;
		case 1:	// g
			color[1] = 255;
			break;
		case 2:	// b
			color[2] = 255;
			break;
		case 3:	// w
			color[3] = 255;
			break;
		default:
			break;
	}
	led_strip_set_pixel_rgbw(led_strip, 0, color[0], color[1], color[2], color[3]);
	led_strip_set_pixel_rgbw(led_strip, 1, color[0], color[1], color[2], color[3]);
	led_strip_set_pixel_rgbw(led_strip, 2, color[0], color[1], color[2], color[3]);
	led_strip_set_pixel_rgbw(led_strip, 3, color[0], color[1], color[2], color[3]);
	ESP_ERROR_CHECK(led_strip_refresh(led_strip));
	return 0;
}

static int led_full_rgbw_color(int mode, uint32_t value)
{
	uint32_t color[4];
	
	memset((void *)&color[0], 0, sizeof(color));
	switch(mode)
	{
		case 0:	// r
			color[0] = value;
			break;
		case 1:	// g
			color[1] = value;
			break;
		case 2:	// b
			color[2] = value;
			break;
		case 3:	// w
			color[3] = value;
			break;
		case 4:	// yellow
			color[0] = value;
			color[1] = value;
			break;
		default:
			break;
	}
	led_strip_set_pixel_rgbw(led_strip, 0, color[0], color[1], color[2], color[3]);
	led_strip_set_pixel_rgbw(led_strip, 1, color[0], color[1], color[2], color[3]);
	led_strip_set_pixel_rgbw(led_strip, 2, color[0], color[1], color[2], color[3]);
	led_strip_set_pixel_rgbw(led_strip, 3, color[0], color[1], color[2], color[3]);
	ESP_ERROR_CHECK(led_strip_refresh(led_strip));
	return 0;
}

static int led_full_white(void)
{
	led_full_rgbw(3);
	return 0;
}

#define LED_ANIMATION_STEP		(1)
static int ani_color = 0;
static int ani_dir = 0;
static int led_animation(int mode, int color)
{
	if(mode == 0)
	{
		// init
		ani_color = 0;
		ani_dir = 0;
	}
	else
	{
		led_full_rgbw_color(color, (uint32_t)ani_color);
		if(ani_dir==0)
		{
            ani_color+= LED_ANIMATION_STEP;
            if(ani_color > 255)
            {
				ani_dir = 1;
				ani_color = 255;
            }
		}
		else
		{
			ani_color -= LED_ANIMATION_STEP;
			if(ani_color < 0)
			{
				ani_dir = 0;
				ani_color = 0;
			}
		}
	}
	return 0;
}

static int led_full_off(void)
{
    // 초기화 직후 모든 LED 끄기
    led_strip_clear(led_strip);
    return 0;
}

static int led_mode = LED_IDLE_MODE;
static int set_led_opmode(int mode)
{
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) 
    {
        led_mode = mode;
        xSemaphoreGive(led_mutex);
    }
    return 0;
}

static int64_t start_tm, end_tm, elapsed;
void led_process_task(void *arg)
{
//	ESP_LOGI(TAG, "%s +", __func__);
//	int64_t start_tm, end_tm, elapsed;
	
    while (1)
    {
    	switch(led_mode)
    	{
			case LED_IDLE_MODE:
				break;
			case LED_NORMAL_MODE:
				led_full_white();
				set_led_opmode(LED_IDLE_MODE);
				break;

			case LED_INITOK_MODE:
				led_full_white();
				start_tm = esp_timer_get_time();
				set_led_opmode(LED_INITOK_MODE_1);
				break;
			case LED_INITOK_MODE_1:
				end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 2000) 
                {
                	led_full_off();
                    start_tm = esp_timer_get_time();
                	set_led_opmode(LED_INITOK_MODE_2);
                }
				break;
            case LED_INITOK_MODE_2:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 2000) 
                {
                    set_led_opmode(LED_IDLE_MODE);
                }
                break;

			case LED_PAIRING_MODE:
				led_full_rgbw(2);
                start_tm = esp_timer_get_time();
                set_led_opmode(LED_PAIRING_MODE_1);
                break;
            case LED_PAIRING_MODE_1:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 500) 
                {
                    led_full_rgbw(0);
                    start_tm = esp_timer_get_time();
                    set_led_opmode(LED_PAIRING_MODE_2);
                }
                break;
            case LED_PAIRING_MODE_2:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 500) 
                {
                    set_led_opmode(LED_PAIRING_MODE);
                }
                break;

			case LED_UNLOCK_MODE:
				led_full_white();
				start_tm = esp_timer_get_time();
				set_led_opmode(LED_LOCK_MODE_1);
				break;
			case LED_UNLOCK_MODE_1:
				end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 500) 
                {
                	led_full_off();
                    start_tm = esp_timer_get_time();
                	set_led_opmode(LED_LOCK_MODE_2);
                }
				break;
            case LED_UNLOCK_MODE_2:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 500) 
                {
                	led_full_white();
                    start_tm = esp_timer_get_time();
                    set_led_opmode(LED_UNLOCK_MODE_3);
                }
                break;
            case LED_UNLOCK_MODE_3:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 500) 
                {
                	led_full_off();
                    start_tm = esp_timer_get_time();
                    set_led_opmode(LED_UNLOCK_MODE_4);
                }
                break;
            case LED_UNLOCK_MODE_4:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 500) 
                {
                    set_led_opmode(LED_IDLE_MODE);
                }
                break;

			case LED_LOCK_MODE:
				led_full_rgbw(0);
				start_tm = esp_timer_get_time();
				set_led_opmode(LED_LOCK_MODE_1);
				break;
			case LED_LOCK_MODE_1:
				end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 2000) 
                {
                	led_full_off();
                    start_tm = esp_timer_get_time();
                	set_led_opmode(LED_LOCK_MODE_2);
                }
				break;
            case LED_LOCK_MODE_2:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 2000) 
                {
                    set_led_opmode(LED_IDLE_MODE);
                }
                break;

			case LED_INVALID_MODE:
				led_full_rgbw(0);
				start_tm = esp_timer_get_time();
				set_led_opmode(LED_INVALID_MODE_1);
				break;
			case LED_INVALID_MODE_1:
				end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 500) 
                {
                	led_full_off();
                    start_tm = esp_timer_get_time();
                	set_led_opmode(LED_INVALID_MODE_2);
                }
				break;
            case LED_INVALID_MODE_2:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 500) 
                {
                	led_full_rgbw(0);
                    start_tm = esp_timer_get_time();
                    set_led_opmode(LED_INVALID_MODE_3);
                }
                break;
            case LED_INVALID_MODE_3:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 500) 
                {
                	led_full_off();
                    start_tm = esp_timer_get_time();
                    set_led_opmode(LED_INVALID_MODE_4);
                }
                break;
            case LED_INVALID_MODE_4:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 500) 
                {
                    set_led_opmode(LED_IDLE_MODE);
                }
                break;

			case LED_INUSE_MODE:
				led_full_rgbw(1);
				set_led_opmode(LED_IDLE_MODE);
				break;

			case LED_CLEANING_MODE:
				led_animation(0, 0);
				led_animation(1, 2);
				set_led_opmode(LED_CLEANING_MODE_1);
				break;
			case LED_CLEANING_MODE_1:
				led_animation(1, 2);
				break;

			case LED_MANAGE_MODE:
				led_animation(0, 0);
				led_animation(1, 4);
				set_led_opmode(LED_MANAGE_MODE_1);
				break;
			case LED_MANAGE_MODE_1:
				led_animation(1, 4);
				break;

			case LED_BINFULL_MODE:
				led_full_rgbw(0);
				set_led_opmode(LED_IDLE_MODE);
				break;

			case LED_ERROR_MODE:
				led_full_rgbw(0);
				start_tm = esp_timer_get_time();
				set_led_opmode(LED_ERROR_MODE_1);
				break;
			case LED_ERROR_MODE_1:
				end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 300) 
                {
                	led_full_off();
                    start_tm = esp_timer_get_time();
                	set_led_opmode(LED_ERROR_MODE_2);
                }
				break;
            case LED_ERROR_MODE_2:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 300) 
                {
                    set_led_opmode(LED_ERROR_MODE);
                }
                break;

			case LED_QCMODE_MODE:
				led_full_white();
				start_tm = esp_timer_get_time();
				set_led_opmode(LED_QCMODE_MODE_1);
				break;
			case LED_QCMODE_MODE_1:
				end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 300) 
                {
                	led_full_off();
                    start_tm = esp_timer_get_time();
                	set_led_opmode(LED_QCMODE_MODE_2);
                }
				break;
            case LED_QCMODE_MODE_2:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 300) 
                {
                    set_led_opmode(LED_QCMODE_MODE);
                }
                break;

			case LED_QCQUIT_MODE:
				led_full_rgbw(1);
				start_tm = esp_timer_get_time();
				set_led_opmode(LED_QCQUIT_MODE_1);
				break;
			case LED_QCQUIT_MODE_1:
				end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 300) 
                {
                	led_full_off();
                    start_tm = esp_timer_get_time();
                	set_led_opmode(LED_QCQUIT_MODE_2);
                }
				break;
            case LED_QCQUIT_MODE_2:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 300) 
                {
                    led_full_rgbw(1);
                    start_tm = esp_timer_get_time();
                    set_led_opmode(LED_QCQUIT_MODE_3);
                }
                break;
            case LED_QCQUIT_MODE_3:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 300) 
                {
                    led_full_off();
                    start_tm = esp_timer_get_time();
                    set_led_opmode(LED_QCQUIT_MODE_4);
                }
                break;
            case LED_QCQUIT_MODE_4:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 300) 
                {
                    led_full_rgbw(1);
                    start_tm = esp_timer_get_time();
                    set_led_opmode(LED_QCQUIT_MODE_5);
                }
                break;
            case LED_QCQUIT_MODE_5:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                if (elapsed >= 300) 
                {
                    led_full_off();
                    set_led_opmode(LED_IDLE_MODE);
                }
                break;
				
			default:
				break;
    	}
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void led_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);

    while (1) {
        message_t msg;
        if (xQueueReceive(led_cmd_msg, &msg, portMAX_DELAY) == pdPASS) {
//          ESP_LOGI(TAG, "[Receiver %ld] transfer complete -> cmd %d ", msg.task_id, msg.cmd);
            switch((int)(msg.cmd))
            {
                case LED_IDLE_CMD:
					set_led_opmode(LED_NORMAL_MODE);
                	break;
                case LED_INITOK_CMD:
					set_led_opmode(LED_INITOK_MODE);
                	break;
                case LED_PAIRING_CMD:
					set_led_opmode(LED_PAIRING_MODE);
                	break;
                case LED_UNLOCK_CMD:
                	set_led_opmode(LED_UNLOCK_MODE);
                	break;
                case LED_LOCK_CMD:
                	set_led_opmode(LED_LOCK_MODE);
                	break;
                case LED_INVALID_CMD:
                	set_led_opmode(LED_INVALID_MODE);
                	break;
                case LED_INUSE_CMD:
                	set_led_opmode(LED_INUSE_MODE);
                	break;
                case LED_CLEANING_CMD:
                	set_led_opmode(LED_CLEANING_MODE);
                	break;
                case LED_MANAGE_CMD:
                	set_led_opmode(LED_MANAGE_MODE);
                	break;
                case LED_BINFULL_CMD:
                	set_led_opmode(LED_BINFULL_MODE);
                	break;
                case LED_ERROR_CMD:
                	set_led_opmode(LED_ERROR_MODE);
                	break;
                case LED_QCMODE_CMD:
                	set_led_opmode(LED_QCMODE_MODE);
                	break;
                case LED_QCQUIT_CMD:
                	set_led_opmode(LED_QCQUIT_MODE);
                	break;

                case LED_FULL_RED_CMD:       // test
                    led_full_rgbw(0);
                	break;
                case LED_FULL_GREEN_CMD:       // test
                    led_full_rgbw(1);
                	break;
                case LED_FULL_BLUE_CMD:       // test
                    led_full_rgbw(2);
                	break;
                case LED_FULL_WHITE_CMD:       // test
                    led_full_rgbw(3);
                	break;
                case LED_FULL_OFF_CMD:       // test
                	led_full_off();
                	break;
                
                default:
                	break;
            }
        }
    }
}

void led_init(void)
{
	ESP_LOGI(TAG, "%s", __func__);

    /* 1. LED 스트립 공통 설정 */
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,
        .max_leds = LED_STRIP_MAX_LEDS,
        .led_model = LED_MODEL_SK6812, 
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRBW,

        .flags = {
            .invert_out = false,
        }
    };

    /* 2. RMT 백엔드 설정 */
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz RMT 클럭 해상도
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false, // ESP32 기본 칩은 RMT DMA를 지원하지 않으므로 false
        }
    };

    /* 3. LED 스트립 장치 할당 및 초기화 */
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    
    // 초기화 직후 모든 LED 끄기
    led_strip_clear(led_strip);

    led_cmd_msg = xQueueCreate(10, sizeof(message_t));
    led_mutex = xSemaphoreCreateMutex();

    xTaskCreate(led_task, "led_task", 2048, NULL, 10, NULL);
    xTaskCreate(led_process_task, "led_process_task", 2048, NULL, 10, NULL);

}
