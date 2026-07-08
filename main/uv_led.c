#include "main.h"

static const char *TAG = "UV_LED";

#define GPIO_UV_LED   	48

static void uv_led_gpio_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << GPIO_UV_LED,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    gpio_set_level(GPIO_UV_LED, 0); // off
}


int uv_led_enable(int enable)
{
	if(enable)
	{
		gpio_set_level(GPIO_UV_LED, 1);
	}
	else
	{
		gpio_set_level(GPIO_UV_LED, 0);
	}
	return 0;
}

#if 0
void uv_led_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);

    while (1) {

        // 10ms 마다 핀 상태를 검사 (샘플링 주기)
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
#endif 

void uv_led_init(void)
{
	ESP_LOGI(TAG, "%s", __func__);
	uv_led_gpio_init();
//    xTaskCreate(uv_led_task, "uv_led_task", 2048, NULL, 10, NULL);
}
