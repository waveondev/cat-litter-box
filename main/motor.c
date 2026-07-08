#include "main.h"

static const char *TAG = "MOTOR";
static SemaphoreHandle_t mt_mutex;
static QueueHandle_t motor_msg = NULL;

void send_motor_msg(void *message, uint32_t cmd)
{
	mt_message_t *msg = (mt_message_t *)message;
    msg->cmd = cmd;
    BaseType_t status = xQueueSend(motor_msg, msg, pdMS_TO_TICKS(100));
    if (status == pdPASS) 
    {
//        ESP_LOGI(TAG, "[Sender %ld] transfer complete -> cmd %d ", msg->task_id, msg->cmd);
    } 
    else 
    {
//        ESP_LOGW(TAG, "[Sender %ld] queuse full transfer failed", msg->task_id);
    }
}

static int64_t start_tm, end_tm, elapsed, pt_status;
static int mt_angle = 0;
static int mt_dir = 0;
static int mt_timeout = 0;
static int mt_pause = 0;
static int mt_mode = MT_IDLE_MODE;
static int mt_mode_backup = MT_IDLE_MODE;
static int mt_plate_run = 0;
static int mt_cal = 0;
static int set_mt_mode(int mode)
{
    if (xSemaphoreTake(mt_mutex, portMAX_DELAY) == pdTRUE) 
    {
        mt_mode = mode;
        xSemaphoreGive(mt_mutex);
    }
    return 0;
}

static int mt_restore_proc(void *arg)
{
    mt_message_t msg = {0};
    msg.task_id = (uint32_t)arg;

	if(mt_plate_run & 0x01)
	{
		if(mt_plate_run & 0x80)
		{
	        send_dc_motor_msg(&msg, PLATE_FORWARD_CMD);
		}
		else if(mt_plate_run & 0x40)
		{
			send_dc_motor_msg(&msg, PLATE_REVERSE_CMD);
		}
	}
    set_mt_mode(mt_mode_backup);
	return 0;
}

static int mt_pause_proc(mt_message_t *msg)
{
    ESP_LOGI(TAG, "%s %d", __func__, mt_mode);
    switch(mt_mode)
    {
		case MAIN_MOTOR_MODE_1:
            send_step_motor_msg(msg, MAIN_STOP_CMD);
			mt_mode_backup = MAIN_MOTOR_MODE;
		break;
		case WASTE_MOTOR_MODE_1:
            send_step_motor_msg(msg, WASTE_STOP_CMD);
			mt_mode_backup = WASTE_MOTOR_MODE;
		break;
		case SCPINOUT_MOTOR_MODE_1:
            send_dc_motor_msg(msg, SCP_INOUT_STOP_CMD);
			mt_mode_backup = SCPINOUT_MOTOR_MODE;
		break;
		case SCPSPIN_MOTOR_MODE_1:
            send_step_motor_msg(msg, SCP_SPIN_STOP_CMD);
			mt_mode_backup = SCPSPIN_MOTOR_MODE;
		break;
		default:
		break;
    }
    if(mt_plate_run)
    {
		send_dc_motor_msg(msg, PLATE_STOP_CMD);
    }
    set_mt_mode(MT_IDLE_MODE);
	return 0;
}

void motor_process_task(void *arg)
{
//	ESP_LOGI(TAG, "%s +", __func__);
	
    mt_message_t msg = {0};
    msg.task_id = (uint32_t)arg;
    
    while (1)
    {
    	switch(mt_mode)
    	{
			case MT_IDLE_MODE:
			break;

			case MAIN_MOTOR_MODE:
				if(!mt_dir)
				{
					send_step_motor_msg(&msg, MAIN_FORWARD_CMD);
				}
				else
				{
					send_step_motor_msg(&msg, MAIN_REVERSE_CMD);
				}
	            start_tm = esp_timer_get_time();
	            set_mt_mode(MAIN_MOTOR_MODE_1);
			break;
			case MAIN_MOTOR_MODE_1:
				end_tm = esp_timer_get_time();
	            elapsed = (end_tm - start_tm) / 1000; 
	            pt_status = get_pt_status();
	            if(mt_pause)
	            {
	            	mt_pause_proc(&msg);
	            }
	            else if(!get_step_motor_run(MAIN_COVER_MOTOR))
	            {
	                send_step_motor_msg(&msg, MAIN_STOP_CMD);
					set_mt_mode(MT_IDLE_MODE);
	            }
	            if (elapsed >= mt_timeout) 
	            {
	            	// timeout
	                send_step_motor_msg(&msg, MAIN_STOP_CMD);
	                set_mt_mode(MT_ERROR_TIMEOUT);
	                break;
	            }
/*				else if(!mt_cal && !mt_dir && (pt_status & PT_BIT_MCOVER_OPEN))
				{
					pt_status &=~(PT_BIT_MCOVER_OPEN); // clear bit
					set_pt_status(pt_status);
	                send_step_motor_msg(&msg, MAIN_STOP_CMD);
					set_mt_mode(MT_IDLE_MODE);
				}
                else if(!mt_cal && mt_dir && (pt_status & PT_BIT_MCOVER_CLOSE))
                {
                    pt_status &=~(PT_BIT_MCOVER_CLOSE); // clear bit
                    set_pt_status(pt_status);
                    send_step_motor_msg(&msg, MAIN_STOP_CMD);
                    set_mt_mode(MT_IDLE_MODE);
                } */
			break;

			case WASTE_MOTOR_MODE:
                if(!mt_dir)
                {
                    send_step_motor_msg(&msg, WASTE_FORWARD_CMD);
                }
                else
                {
                    send_step_motor_msg(&msg, WASTE_REVERSE_CMD);
                }
                start_tm = esp_timer_get_time();
                set_mt_mode(WASTE_MOTOR_MODE_1);
            break;
            case WASTE_MOTOR_MODE_1:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                pt_status = get_pt_status();
	            if(mt_pause)
	            {
	            	mt_pause_proc(&msg);
	            }
	            else if(!get_step_motor_run(WASTE_COVER_MOTOR))
	            {
	                send_step_motor_msg(&msg, WASTE_STOP_CMD);
					set_mt_mode(MT_IDLE_MODE);
	            }
                if (elapsed >= mt_timeout) 
                {
                    // timeout
                    send_step_motor_msg(&msg, WASTE_STOP_CMD);
                    set_mt_mode(MT_ERROR_TIMEOUT);
                    break;
                }
/*                else if(!mt_cal && !mt_dir && (pt_status & PT_BIT_WASTE_OPEN))
                {
                    pt_status &=~(PT_BIT_WASTE_OPEN); // clear bit
                    set_pt_status(pt_status);
                    send_step_motor_msg(&msg, WASTE_STOP_CMD);
                    set_mt_mode(MT_IDLE_MODE);
                }
                else if(!mt_cal && mt_dir && (pt_status & PT_BIT_WASTE_CLOSE))
                {
                    pt_status &=~(PT_BIT_WASTE_CLOSE); // clear bit
                    set_pt_status(pt_status);
                    send_step_motor_msg(&msg, WASTE_STOP_CMD);
                    set_mt_mode(MT_IDLE_MODE);
                } */
            break;

			case SCPINOUT_MOTOR_MODE:
                if(!mt_dir)
                {
                    send_dc_motor_msg(&msg, SCP_INOUT_FORWARD_CMD);
                }
                else
                {
                    send_dc_motor_msg(&msg, SCP_INOUT_REVERSE_CMD);
                }
                start_tm = esp_timer_get_time();
                set_mt_mode(SCPINOUT_MOTOR_MODE_1);
			break;
			case SCPINOUT_MOTOR_MODE_1:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                pt_status = get_pt_status();
	            if(mt_pause)
	            {
	            	mt_pause_proc(&msg);
	            }
                if (elapsed >= mt_timeout) 
                {
                    // timeout
	            	ESP_LOGI(TAG, "SCPINOUT_MOTOR_MODE_1 timeout");
                    send_dc_motor_msg(&msg, SCP_INOUT_STOP_CMD);
                    set_mt_mode(MT_ERROR_TIMEOUT);
                    break;
                }
                else if(!mt_cal && !mt_dir && (pt_status & PT_BIT_SCP_OUT))
                {
	            	ESP_LOGI(TAG, "PT_BIT_SCP_OUT");
                    pt_status &=~(PT_BIT_SCP_OUT); // clear bit
                    set_pt_status(pt_status);
                    send_dc_motor_msg(&msg, SCP_INOUT_STOP_CMD);
                    set_mt_mode(MT_IDLE_MODE);
                }
                else if(!mt_cal && mt_dir && (pt_status & PT_BIT_SCP_IN))
                {
	            	ESP_LOGI(TAG, "PT_BIT_SCP_IN");
                    pt_status &=~(PT_BIT_SCP_IN); // clear bit
                    set_pt_status(pt_status);
                    send_dc_motor_msg(&msg, SCP_INOUT_STOP_CMD);
                    set_mt_mode(MT_IDLE_MODE);
                }
			break;

			case SCPSPIN_MOTOR_MODE:
                if(!mt_dir)
                {
                	msg.angle = mt_angle;
                    send_step_motor_msg(&msg, SCP_SPIN_FORWARD_CMD);
                }
                else
                {
                	msg.angle = mt_angle;
                    send_step_motor_msg(&msg, SCP_SPIN_REVERSE_CMD);
                }
                start_tm = esp_timer_get_time();
                set_mt_mode(SCPSPIN_MOTOR_MODE_1);
            break;
            case SCPSPIN_MOTOR_MODE_1:
                end_tm = esp_timer_get_time();
                elapsed = (end_tm - start_tm) / 1000; 
                pt_status = get_pt_status();
	            if(mt_pause)
	            {
	            	mt_pause_proc(&msg);
	            }
	            else if(!get_step_motor_run(SCP_SPIN_MOTOR))
	            {
	                send_step_motor_msg(&msg, SCP_SPIN_STOP_CMD);
					set_mt_mode(MT_IDLE_MODE);
	            }
                if (elapsed >= mt_timeout) 
                {
                    // timeout
                    send_step_motor_msg(&msg, SCP_SPIN_STOP_CMD);
                    set_mt_mode(MT_ERROR_TIMEOUT);
                    break;
                }
/*                else if(!mt_cal && !mt_dir && (pt_status & PT_BIT_SCP_SPIN_ST))
                {
                    pt_status &=~(PT_BIT_SCP_SPIN_ST); // clear bit
                    set_pt_status(pt_status);
                    send_step_motor_msg(&msg, SCP_SPIN_STOP_CMD);
                    set_mt_mode(MT_IDLE_MODE);
                }
                else if(!mt_cal && mt_dir && (pt_status & PT_BIT_SCP_SPIN_ED))
                {
                    pt_status &=~(PT_BIT_SCP_SPIN_ED); // clear bit
                    set_pt_status(pt_status);
                    send_step_motor_msg(&msg, SCP_SPIN_STOP_CMD);
                    set_mt_mode(MT_IDLE_MODE);
                } */
            break;

			case MT_ERROR_TIMEOUT:
				break;
				
			default:
				break;
    	}
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int check_motor(void)
{
	int ret = 1;
	if(mt_pause)
	{
		ret = 1;
	}
	else if(mt_mode == MT_IDLE_MODE)
	{
		ret = 0;
	}
	else if(mt_mode == MT_ERROR_TIMEOUT)
	{
		ret = -1;
	}
	return ret;	// running
}

// 메인 태스크 루프
void motor_task(void *arg) 
{
    ESP_LOGI(TAG, "%s +", __func__);
    while (1) {
        mt_message_t msg = {0};
        if (xQueueReceive(motor_msg, &msg, portMAX_DELAY) == pdPASS) {
//          ESP_LOGI(TAG, "[Receiver %ld] transfer complete -> cmd %d ", msg.task_id, msg.cmd);
            switch((int)(msg.cmd))
            {
                case MT_PLATE_CMD:
                	mt_plate_run = 0;
	                if(msg.direction)
	                {
						send_dc_motor_msg(&msg, PLATE_FORWARD_CMD);
						mt_plate_run = 1 | 0x80;
					}
					else
					{
						send_dc_motor_msg(&msg, PLATE_REVERSE_CMD);
						mt_plate_run = 1 | 0x40;
					}
                break;
                case MT_PLATE_STOP_CMD:
                	send_dc_motor_msg(&msg, PLATE_STOP_CMD);
                    mt_plate_run = 0;
                break;
                case MT_MAIN_CMD:
                	mt_angle = (int)msg.angle;
                	mt_dir = (int)msg.direction;
                	mt_timeout = (int)msg.timeout;
                    set_mt_mode(MAIN_MOTOR_MODE);
                break;
                case MT_WASTE_CMD:
                	mt_angle = (int)msg.angle;
                	mt_dir = (int)msg.direction;
                	mt_timeout = (int)msg.timeout;
                    set_mt_mode(WASTE_MOTOR_MODE);
                break;
                case MT_SCP_INOUT_CMD:
                	mt_angle = (int)msg.angle;
                	mt_dir = (int)msg.direction;
                	mt_timeout = (int)msg.timeout;
	            	ESP_LOGI(TAG, "MT_SCP_INOUT_CMD %d %d %d", mt_angle, mt_dir, mt_timeout);
                    set_mt_mode(SCPINOUT_MOTOR_MODE);
                break;
                case MT_SCP_SPIN_CMD:
                	mt_angle = (int)msg.angle;
                	mt_dir = (int)msg.direction;
                	mt_timeout = (int)msg.timeout;
                	mt_cal = (int)msg.cal;
                    set_mt_mode(SCPSPIN_MOTOR_MODE);
                break;
                case MT_PAUSE_CMD:
                	mt_pause = 1;
                break;
                case MT_RESTORE_CMD:
                	mt_restore_proc(arg);
                	mt_pause = 0;
                break;
                case MT_RESET_CMD:
                    mt_angle = 0;
                    mt_dir = 0;
                    mt_timeout = 0;
                    mt_pause = 0;
                    mt_mode = MT_IDLE_MODE;
                    mt_mode_backup = MT_IDLE_MODE;
                    mt_plate_run = 0;
                    mt_cal = 0;
                break;
                default:
                break;
            }
        }
    }
//    ESP_LOGI(TAG, "%s -", __func__);
    vTaskDelete(NULL);
}

void motor_task_init(void)
{
	ESP_LOGI(TAG, "%s", __func__);

    motor_msg = xQueueCreate(10, sizeof(mt_message_t));
    mt_mutex = xSemaphoreCreateMutex();
    
    xTaskCreate(motor_task, "motor_task", 2*2048, NULL, 10, NULL);
    xTaskCreate(motor_process_task, "motor_process_task", 2*2048, NULL, 10, NULL);
}
