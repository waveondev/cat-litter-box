#include "main.h"

static const char *TAG = "UV_LED";
static bool ev_led_enable = false;
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

void uv_led_task(void *arg)
{
	int pt;
	ESP_LOGI(TAG, "%s +", __func__);

    while (1) {
        pt = get_pt_status();
		if(!(pt&PT_BIT_REED_SW) && (pt&PT_BIT_WASTE_CLOSE))
		{
			if(!ev_led_enable)
			{
                ESP_LOGI(TAG, "UV LED ENABLED");
				uv_led_enable(1);
				ev_led_enable = true;
			}
		}
		else
		{
			if(ev_led_enable)
			{
                ESP_LOGI(TAG, "UV LED DISABLED");
				uv_led_enable(0);
				ev_led_enable = false;
			}
		}
			
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void uv_led_init(void)
{
	int pt;
	ESP_LOGI(TAG, "%s", __func__);
	
	uv_led_gpio_init();
    pt = get_pt_status();
    if(!(pt&PT_BIT_REED_SW) && (pt&PT_BIT_WASTE_CLOSE))
    {
        ESP_LOGI(TAG, "UV LED ENABLED");
		uv_led_enable(1);
		ev_led_enable = true;
    }
    else
    {
        ESP_LOGI(TAG, "UV LED DISABLED");
		uv_led_enable(0);
        ev_led_enable = false;
	}
    xTaskCreate(uv_led_task, "uv_led_task", 2048, NULL, 10, NULL);
    
}
