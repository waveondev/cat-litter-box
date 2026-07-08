#include "main.h"

static const char *TAG = "KEYSCAN";
// 사용할 GPIO 핀 정의
#define KEY_CLEAN    14
#define KEY_CHANGE    15
#define KEY_SET    16
#define GPIO_BIT_MASK  ((1ULL << KEY_CLEAN) | (1ULL << KEY_CHANGE) | (1ULL << KEY_SET))

// 시간 임계값 정의 (단위: 밀리초)
#define TIME_THRES_3S   3000
#define TIME_THRES_5S   5000
#define TIME_THRES_10S  10000

// 각 핀의 상태를 관리하기 위한 구조체
typedef struct {
    gpio_num_t pin;
    bool is_pressed;
    int64_t press_start_time;
    bool event_triggered_3s;
    bool event_triggered_5s;
    bool event_triggered_10s;
} key_state_t;

// GPIO 초기화 함수
static void init_key_gpio(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = GPIO_BIT_MASK,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,     // Low Active이므로 내부 풀업 활성화
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE        // 주기적 폴링 방식으로 처리 (바운싱 제어 용이)
    };
    gpio_config(&io_conf);
}

// 키 입력 처리 태스크
static void key_polling_task(void *arg)
{
    message_t msg;

    key_state_t keys[] = {
        {.pin = KEY_CLEAN, .is_pressed = false, .press_start_time = 0},
        {.pin = KEY_CHANGE, .is_pressed = false, .press_start_time = 0},
        {.pin = KEY_SET, .is_pressed = false, .press_start_time = 0}
    };
    const int num_keys = sizeof(keys) / sizeof(keys[0]);

    ESP_LOGI(TAG, "%s +", __func__);

    while (1) {
        // 20ms 주기로 상태 체크 (소프트웨어 디바운싱 효과)
        vTaskDelay(pdMS_TO_TICKS(20));

        int64_t current_time = esp_timer_get_time() / 1000; // 현재 시간을 ms 단위로 가져옴

        for (int i = 0; i < num_keys; i++) {
            // Low Active: 버튼이 눌리면 레벨이 0(Low)이 됨
            bool current_level = gpio_get_level(keys[i].pin) == 0;

            if (current_level) {
                // 처음 눌린 경우
                if (!keys[i].is_pressed) {
                    keys[i].is_pressed = true;
                    keys[i].press_start_time = current_time;
                    keys[i].event_triggered_3s = false;
                    keys[i].event_triggered_5s = false;
                    keys[i].event_triggered_10s = false;
                    ESP_LOGD(TAG, "GPIO %d Pressed", keys[i].pin);
                } else {
                    // 버튼이 계속 눌려있는 상태 -> 누른 시간 계산
                    int64_t duration = current_time - keys[i].press_start_time;

                    // 10초 이상 이벤트 발생 (최초 1회만)
                    if (duration >= TIME_THRES_10S && !keys[i].event_triggered_10s) {
                        keys[i].event_triggered_10s = true;
                        if(keys[1].event_triggered_10s && keys[2].event_triggered_10s)
                        {
                            msg.task_id = (uint32_t)arg;
                            send_ui_cmd_msg(&msg, UI_FACTORY_CMD);
                        }
                    }
                    // 5초 이상 이벤트 발생 (최초 1회만)
                    else if (duration >= TIME_THRES_5S && !keys[i].event_triggered_5s) {
                        keys[i].event_triggered_5s = true;
                        if(keys[i].pin == KEY_SET && !keys[0].is_pressed && !keys[1].is_pressed)
                        {
                            msg.task_id = (uint32_t)arg;
                            send_ui_cmd_msg(&msg, UI_PAIRING_CMD);
						}
                    }
                    // 3초 이상 이벤트 발생 (최초 1회만)
                    else if (duration >= TIME_THRES_3S && !keys[i].event_triggered_3s) {
                        keys[i].event_triggered_3s = true;
                        if(keys[i].pin == KEY_CHANGE && !keys[0].is_pressed && !keys[2].is_pressed)
                        {
                            msg.task_id = (uint32_t)arg;
                            send_ui_cmd_msg(&msg, UI_MANAGE_START_CMD);
						}
                    }
                }
            } else {
                // 버튼이 떨어졌을 때 처리
                if (keys[i].is_pressed) {
//                    int64_t total_duration = current_time - keys[i].press_start_time;

					if(!keys[i].event_triggered_10s &&
						!keys[i].event_triggered_5s &&
						!keys[i].event_triggered_3s)
					{
						if(keys[i].pin == KEY_CLEAN && !keys[1].is_pressed && !keys[2].is_pressed)
						{
                            msg.task_id = (uint32_t)arg;
                            send_ui_cmd_msg(&msg, UI_CLEAN_CMD);
						}
						else if(keys[i].pin == KEY_CHANGE && !keys[0].is_pressed && !keys[2].is_pressed)
						{
                            msg.task_id = (uint32_t)arg;
                            send_ui_cmd_msg(&msg, UI_MANAGE_FINISH_CMD);
						}
						else if(keys[i].pin == KEY_SET && !keys[0].is_pressed && !keys[1].is_pressed)
						{
                            msg.task_id = (uint32_t)arg;
                            send_ui_cmd_msg(&msg, UI_TARE_ZERO_CMD);
						}
					}
//                    ESP_LOGI(TAG, "GPIO %d Released. Total duration: %lld ms", keys[i].pin, total_duration);
                    
                    // 상태 초기화
                    keys[i].is_pressed = false;
	                keys[i].event_triggered_3s = false;
	                keys[i].event_triggered_5s = false;
	                keys[i].event_triggered_10s = false;
                }
            }
        }
    }
}

void keyscan_init(void)
{
	ESP_LOGI(TAG, "%s", __func__);
    init_key_gpio();
    xTaskCreate(key_polling_task, "key_polling_task", 3072, NULL, 5, NULL);
}

