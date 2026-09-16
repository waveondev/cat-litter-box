#include "main.h"

static const char *TAG = "UV_LED";
static bool uv_led_f = false;
static int uv_led_tm = -1;
#define GPIO_UV_LED   	48
#define UVLED_ON_TIMEOUT			1200
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

static int uv_led_enable(bool enable)
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

static bool check_uvled(void)
{
	int pt;
    pt = get_pt_status();
	if(!(pt&PT_BIT_REED_SW) && (pt&PT_BIT_WASTE_CLOSE))
	{
		return true;
	}
	return false;
}

void uv_led_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);

    while (1) {
        if(check_uvled())
        {
            ESP_LOGI(TAG, "UV LED ENABLED");
            uv_led_enable(true);
            uv_led_f = true;
        }
        else
        {
            ESP_LOGI(TAG, "UV LED DISABLED");
            uv_led_enable(false);
            uv_led_f = false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void uv_led_init(void)
{
	ESP_LOGI(TAG, "%s", __func__);

	uv_led_gpio_init();
    if(check_uvled())
    {
        ESP_LOGI(TAG, "UV LED ENABLED");
		uv_led_enable(true);
        uv_led_f = true;
    }
    else
    {
        ESP_LOGI(TAG, "UV LED DISABLED");
		uv_led_enable(false);
        uv_led_f = false;
	}
    //xTaskCreate(uv_led_task, "uv_led_task", 2048, NULL, 10, NULL);

}
