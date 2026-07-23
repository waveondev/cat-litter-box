#include "main.h"

static const char *TAG = "UI";

static bool clean_task_run = false;
static bool manage_finish_task_run = false;
static bool manage_start_task_run = false;
static bool pairing_task_run = false;
static bool factory_task_run = false;
static bool tare_task_run = false;
//static bool ui_mt_pause_f = false;
static bool ui_manage_f = false;

static QueueHandle_t ui_cmd_msg = NULL;

void send_ui_cmd_msg(void *message, uint32_t cmd)
{
	message_t *msg = (message_t *)message;
    msg->cmd = cmd;
    BaseType_t status = xQueueSend(ui_cmd_msg, msg, pdMS_TO_TICKS(100));
    if (status == pdPASS) 
    {
//        ESP_LOGI(TAG, "[Sender %ld] transfer complete -> cmd %d ", msg->task_id, msg->cmd);
    } 
    else 
    {
//        ESP_LOGW(TAG, "[Sender %ld] queuse full transfer failed", msg->task_id);
    }
}

void ui_clean_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);

	clean_task_run = true;
	do_clean(arg);
	clean_task_run = false;

	ESP_LOGI(TAG, "%s -", __func__);
	vTaskDelete(NULL);
}

void ui_manage_finish_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);
	manage_finish_task_run = true;

    message_t led_msg;
    led_msg.task_id = (uint32_t)arg;
	send_led_cmd_msg(&led_msg, LED_MANAGE_CMD);
	do_manage_finish(arg);
	loadcell_init();	// tare zero 
	send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
	
    ui_manage_f = false;

	manage_finish_task_run = false;
	vTaskDelete(NULL);
}
void ui_manage_start_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);
	manage_start_task_run = true;

    message_t led_msg;
    led_msg.task_id = (uint32_t)arg;
    send_led_cmd_msg(&led_msg, LED_MANAGE_CMD);
	do_manage_start(arg);
    send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
    
	manage_start_task_run = false;
	vTaskDelete(NULL);
}
void ui_tarezero_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);
	tare_task_run = true;
    loadcell_init();	// tare zero 
	vTaskDelay(pdMS_TO_TICKS(1000));
	
	tare_task_run = false;
	vTaskDelete(NULL);
}
void ui_pairing_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);
	pairing_task_run = true;
    message_t led_msg;
    led_msg.task_id = (uint32_t)arg;
    
    send_led_cmd_msg(&led_msg, LED_PAIRING_CMD);

	vTaskDelay(pdMS_TO_TICKS(5000));

	send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
	
	pairing_task_run = false;
	vTaskDelete(NULL);
}
void ui_factory_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);
	factory_task_run = true;

    message_t led_msg;
    led_msg.task_id = (uint32_t)arg;
    send_led_cmd_msg(&led_msg, LED_QCMODE_CMD);
	
	vTaskDelay(pdMS_TO_TICKS(5000));
	
    send_led_cmd_msg(&led_msg, LED_QCQUIT_CMD);
	
	factory_task_run = false;
	vTaskDelete(NULL);
}

void ui_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);

    UBaseType_t remaining_stack = uxTaskGetStackHighWaterMark(NULL); 
    ESP_LOGI(TAG, "Remaining stack: %d bytes", remaining_stack);
	
	while(1)
	{
        message_t msg;
        if (xQueueReceive(ui_cmd_msg, &msg, portMAX_DELAY) == pdPASS) 
        {
//          ESP_LOGI(TAG, "[Receiver %ld] transfer complete -> cmd %d ", msg.task_id, msg.cmd);
	        switch((int)(msg.cmd))
	        {
				case UI_CLEAN_CMD:
                	ESP_LOGI(TAG, "UI_CLEAN_CMD");
                	if(!clean_task_run)
                	{
						xTaskCreate(ui_clean_task, "ui_clean_task", 4096, NULL, 10, NULL);
                	}
					break;
				case UI_MANAGE_FINISH_CMD:
                	ESP_LOGI(TAG, "UI_MANAGE_FINISH_CMD");
                	if(!manage_finish_task_run)
                	{
                		if(ui_manage_f)
                		{
							xTaskCreate(ui_manage_finish_task, "ui_manage_finish_task", 4096, NULL, 10, NULL);
						}
                	}
					break;
				case UI_TARE_ZERO_CMD:
                	ESP_LOGI(TAG, "UI_TARE_ZERO_CMD");
                	if(!tare_task_run)
                	{
						xTaskCreate(ui_tarezero_task, "ui_tarezero_task", 4096, NULL, 10, NULL);
                	}
					break;
				case UI_MANAGE_START_CMD:
                	ESP_LOGI(TAG, "UI_MANAGE_START_CMD");
                	if(!manage_start_task_run)
                	{
                		if(!ui_manage_f)
                		{
							xTaskCreate(ui_manage_start_task, "ui_manage_start_task", 4096, NULL, 10, NULL);
                            ui_manage_f = true;
						}
                	}
					break;
				case UI_PAIRING_CMD:
                	ESP_LOGI(TAG, "UI_PAIRING_CMD");
                	if(!pairing_task_run)
                	{
						xTaskCreate(ui_pairing_task, "ui_pairing_task", 4096, NULL, 10, NULL);
                	}
					break;
				case UI_FACTORY_CMD:
                	ESP_LOGI(TAG, "UI_FACTORY_CMD");
                	if(!factory_task_run)
                	{
						xTaskCreate(ui_factory_task, "ui_factory_task", 4096, NULL, 10, NULL);
                	}
					break;
	                
	            default:
	            	break;
			}
		}
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void ui_init(void)
{
	ESP_LOGI(TAG, "%s", __func__);	
	ui_cmd_msg = xQueueCreate(10, sizeof(message_t));
    xTaskCreate(ui_task, "ui_task", 2*4096, NULL, 10, NULL);

    // idle led display
    message_t msg;
    send_led_cmd_msg(&msg, LED_IDLE_CMD);
}
