#include "main.h"

static const char *TAG = "UI";

static int clean_task_run = 0;
static int manage_finish_task_run = 0;
static int manage_start_task_run = 0;
static int pairing_task_run = 0;
static int factory_task_run = 0;
static int tare_task_run = 0;
static int ui_mt_pause_f = 0;
static int ui_manage_f = 0;

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

static int wait_motor_operation(void)
{
	int ret = 0;
    do{
        vTaskDelay(pdMS_TO_TICKS(10));
        ret = check_motor();
        if(ret == 1)
        {
            continue;
        }
        else if(ret == 0)
        {
            break;
        }
        else if(ret < 0)
        {
            ESP_LOGI(TAG, "Motor operation timeout error !!");
            break;
        }
    } while(1);
	return ret;
}

int scp_spin_cal(void *arg)
{
	int pt, ret;
    mt_message_t msg = {0};
    msg.task_id = (uint32_t)arg;
    send_motor_msg(&msg, MT_RESET_CMD); // reset 
#if 0
    ESP_LOGI(TAG, "main cover open ");
    msg.angle = 0;  // none
    msg.direction = 0;// forward
    msg.timeout = 10000; // 10 sec
    send_motor_msg(&msg, MT_MAIN_CMD);
    wait_motor_operation();
#endif
    ESP_LOGI(TAG, "scoop out ");
    msg.angle = 0;  // none
    msg.direction = 0;// forward
    msg.timeout = 10000; // 10 sec
    send_motor_msg(&msg, MT_SCP_INOUT_CMD);
    wait_motor_operation();

    ESP_LOGI(TAG, "scoop spin reverse 360 degree for cal");
    msg.angle = 360;    
    msg.direction = 0;// forward
    msg.timeout = 60000; // 60 sec, take a guess
	msg.cal = 1;
    send_motor_msg(&msg, MT_SCP_SPIN_CMD);
    do{
        vTaskDelay(pdMS_TO_TICKS(10));
        pt = get_pt_status();
		if((pt&PT_BIT_SCP_SPIN_ST) && (pt&PT_BIT_SCP_SPIN_ED))
		{
			// check origin angle
			ESP_LOGI(TAG, "check origin angle");
			send_motor_msg(&msg, MT_PAUSE_CMD);
			break;
		}
        ret = check_motor();
        if(ret == 1)
        {
            continue;
        }
        else if(ret == 0)
        {
            break;
        }
        else if(ret < 0)
        {
        	// timeout
            ESP_LOGI(TAG, "Error : not found origin ");
            return -3;
        }
    } while(1);
	msg.cal = 0;
    return 0;
}

void ui_clean_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);
	clean_task_run = 1;
//	while(1)
	{
	    mt_message_t msg = {0};
	    message_t led_msg;
	    msg.task_id = (uint32_t)arg;
	    led_msg.task_id = (uint32_t)arg;

        send_led_cmd_msg(&led_msg, LED_CLEANING_CMD);

	    send_motor_msg(&msg, MT_RESET_CMD); // reset 

        ESP_LOGI(TAG, "Main plate run ");
        msg.angle = 0;	// none
        msg.direction = 0;// forward
        msg.timeout = 0;	// no timeout
	    send_motor_msg(&msg, MT_PLATE_CMD);
#if 0
        ESP_LOGI(TAG, "main cover open ");
        msg.angle = 0;	// none
        msg.direction = 0;// forward
        msg.timeout = 10000; // 10 sec
	    send_motor_msg(&msg, MT_MAIN_CMD);
	    wait_motor_operation();
#endif

        ESP_LOGI(TAG, "scoop spin 150 degree ");
        msg.angle = 150;
        msg.direction = 0;// forward
        msg.timeout = 15000; // 10 sec
	    send_motor_msg(&msg, MT_SCP_SPIN_CMD);
	    wait_motor_operation();

        ESP_LOGI(TAG, "scoop out ");
        msg.angle = 0;	// none
        msg.direction = 0;// forward
        msg.timeout = 10000; // 10 sec
	    send_motor_msg(&msg, MT_SCP_INOUT_CMD);
	    wait_motor_operation();

        ESP_LOGI(TAG, "scoop spin forward 80 degree ");
        msg.angle = 80;
        msg.direction = 0;// forward
        msg.timeout = 10000; // 10 sec
	    send_motor_msg(&msg, MT_SCP_SPIN_CMD);
	    wait_motor_operation();

//        ESP_LOGI(TAG, "wait run plate .... ");
        vTaskDelay(pdMS_TO_TICKS(10000));	// wait 30 sec, until one turn finished
//        send_motor_msg(&msg, MT_PLATE_STOP_CMD); // stop plate

        ESP_LOGI(TAG, "scoop spin reverse 80 degree");
        msg.angle = 80;	
        msg.direction = 1;// reverse
        msg.timeout = 10000; // 10 sec
	    send_motor_msg(&msg, MT_SCP_SPIN_CMD);
	    wait_motor_operation();

        ESP_LOGI(TAG, "waste cover open");
        msg.angle = 0;	// none
        msg.direction = 0;// forward
        msg.timeout = 15000; // 10 sec
	    send_motor_msg(&msg, MT_WASTE_CMD);
	    wait_motor_operation();

        ESP_LOGI(TAG, "scoop in");
        msg.angle = 0;	
        msg.direction = 1;// reverse
        msg.timeout = 10000; // 10 sec
	    send_motor_msg(&msg, MT_SCP_INOUT_CMD);
	    wait_motor_operation();
	    
        ESP_LOGI(TAG, "scoop spin reverse 150 degree");
        msg.angle = 150;	
        msg.direction = 1;// reverse
        msg.timeout = 15000; // 15 sec
	    send_motor_msg(&msg, MT_SCP_SPIN_CMD);
	    wait_motor_operation();

//        vTaskDelay(pdMS_TO_TICKS(1000));	// wait 1 sec

        ESP_LOGI(TAG, "scoop spin forward 150 degree");
        msg.angle = 150;	
        msg.direction = 0;// forward
        msg.timeout = 15000; // 15 sec
	    send_motor_msg(&msg, MT_SCP_SPIN_CMD);
	    wait_motor_operation();

        ESP_LOGI(TAG, "scoop out");
        msg.angle = 0;	
        msg.direction = 0;// forward
        msg.timeout = 10000; // 10 sec
	    send_motor_msg(&msg, MT_SCP_INOUT_CMD);
	    wait_motor_operation();

        ESP_LOGI(TAG, "waste cover close");
        msg.angle = 0;	// none
        msg.direction = 1;// reverse
        msg.timeout = 15000; // 10 sec
	    send_motor_msg(&msg, MT_WASTE_CMD);
	    wait_motor_operation();

        ESP_LOGI(TAG, "scoop in");
        msg.angle = 0;	
        msg.direction = 1;// reverse
        msg.timeout = 10000; // 10 sec
	    send_motor_msg(&msg, MT_SCP_INOUT_CMD);
        wait_motor_operation();

        ESP_LOGI(TAG, "scoop spin reverse 150 degree ");
        msg.angle = 150;	
        msg.direction = 1;// reverse
        msg.timeout = 15000; // 15 sec
	    send_motor_msg(&msg, MT_SCP_SPIN_CMD);
        wait_motor_operation();
        
#if 0
        ESP_LOGI(TAG, "main cover close");
        msg.angle = 0;	// none
        msg.direction = 1;// reverse
        msg.timeout = 10000; // 10 sec
	    send_motor_msg(&msg, MT_MAIN_CMD);
        wait_motor_operation();
#endif

		send_motor_msg(&msg, MT_PLATE_STOP_CMD); // stop plate

		send_led_cmd_msg(&led_msg, LED_IDLE_CMD);

		vTaskDelay(pdMS_TO_TICKS(10));
	}
	clean_task_run = 0;
	ESP_LOGI(TAG, "%s -", __func__);
	vTaskDelete(NULL);
}

void ui_manage_finish_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);
	manage_finish_task_run = 1;

    mt_message_t msg = {0};
    message_t led_msg;
    msg.task_id = (uint32_t)arg;
    led_msg.task_id = (uint32_t)arg;

    send_led_cmd_msg(&led_msg, LED_MANAGE_CMD);

    ESP_LOGI(TAG, "scoop in");
    msg.angle = 0;  
    msg.direction = 1;// reverse
    msg.timeout = 10000; // 10 sec
    send_motor_msg(&msg, MT_SCP_INOUT_CMD);
    wait_motor_operation();

    ESP_LOGI(TAG, "scoop spin reverse 150 degree ");
    msg.angle = 150;    
    msg.direction = 1;// reverse
    msg.timeout = 15000; // 15 sec
    send_motor_msg(&msg, MT_SCP_SPIN_CMD);
    wait_motor_operation();
    
    send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
    
#if 0
    ESP_LOGI(TAG, "main cover close");
    msg.angle = 0;  // none
    msg.direction = 1;// reverse
    msg.timeout = 10000; // 10 sec
    send_motor_msg(&msg, MT_MAIN_CMD);
    wait_motor_operation();
#endif
   	loadcell_init();	// tare zero 
	manage_finish_task_run = 0;
	vTaskDelete(NULL);
}
void ui_manage_start_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);
	manage_start_task_run = 1;
//  while(1)
    {
        mt_message_t msg = {0};
        message_t led_msg;
        msg.task_id = (uint32_t)arg;
        led_msg.task_id = (uint32_t)arg;

        send_led_cmd_msg(&led_msg, LED_MANAGE_CMD);

        send_motor_msg(&msg, MT_RESET_CMD); // reset 

#if 0
        ESP_LOGI(TAG, "main cover open ");
        msg.angle = 0;  // none
        msg.direction = 0;// forward
        msg.timeout = 10000; // 10 sec
        send_motor_msg(&msg, MT_MAIN_CMD);
        wait_motor_operation();
#endif

        ESP_LOGI(TAG, "scoop spin 150 degree ");
        msg.angle = 150;
        msg.direction = 0;// forward
        msg.timeout = 15000; // 10 sec
        send_motor_msg(&msg, MT_SCP_SPIN_CMD);
        wait_motor_operation();

        ESP_LOGI(TAG, "scoop out ");
        msg.angle = 0;  // none
        msg.direction = 0;// forward
        msg.timeout = 10000; // 10 sec
        send_motor_msg(&msg, MT_SCP_INOUT_CMD);
        wait_motor_operation();

        send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
    }
	manage_start_task_run = 0;
	vTaskDelete(NULL);
}
void ui_tarezero_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);
	tare_task_run = 1;
    loadcell_init();	// tare zero 
	vTaskDelay(pdMS_TO_TICKS(1000));
	
	tare_task_run = 0;
	vTaskDelete(NULL);
}
void ui_pairing_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);
	pairing_task_run = 1;
    message_t led_msg;
    led_msg.task_id = (uint32_t)arg;
    
    send_led_cmd_msg(&led_msg, LED_PAIRING_CMD);

	vTaskDelay(pdMS_TO_TICKS(5000));

	send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
	
	pairing_task_run = 0;
	vTaskDelete(NULL);
}
void ui_factory_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);
	factory_task_run = 1;
	while(1)
	{
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	factory_task_run = 0;
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
                	else
                	{
                		if(!ui_mt_pause_f)
                		{
							send_motor_msg(&msg, MT_PAUSE_CMD);
							ui_mt_pause_f = 1;
						}
						else
						{
                            send_motor_msg(&msg, MT_RESTORE_CMD);
                            ui_mt_pause_f = 0;
						}
                	}
					break;
				case UI_MANAGE_FINISH_CMD:
                	ESP_LOGI(TAG, "UI_MANAGE_FINISH_CMD");
                	if(!manage_finish_task_run)
                	{
                		if(ui_manage_f)
                		{
							xTaskCreate(ui_manage_finish_task, "ui_manage_finish_task", 4096, NULL, 10, NULL);
							ui_manage_f = 0;
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
                            ui_manage_f = 1;
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
