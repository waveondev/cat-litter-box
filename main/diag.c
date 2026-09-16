#include "main.h"

static const char *TAG = "DIAG";

static QueueHandle_t diag_cmd_msg = NULL;

static bool diag_status_f = false;
static bool diag_next_step_f = false;
static bool diag_next_func_f = false;
static int diag_func = 0;

bool get_status_diag(void)
{
	return diag_status_f;
}

void set_status_diag(bool enable)
{
	diag_status_f = enable;
}

void send_diag_cmd_msg(void *message, uint32_t cmd)
{
	message_t *msg = (message_t *)message;
    msg->cmd = cmd;
    BaseType_t status = xQueueSend(diag_cmd_msg, msg, pdMS_TO_TICKS(100));
    if (status == pdPASS) 
    {
//        ESP_LOGI(TAG, "[Sender %ld] transfer complete -> cmd %d ", msg->task_id, msg->cmd);
    } 
    else 
    {
//        ESP_LOGW(TAG, "[Sender %ld] queue full transfer failed", msg->task_id);
    }
}

static int wait_diag_operation(int sel, int mt, int timeout, int status)
{
	vTaskDelay(pdMS_TO_TICKS(40));	// for task switch delay
	
    int64_t start, end, elapsed;
    start = esp_timer_get_time();
	int ret = 0;
    do{
        vTaskDelay(pdMS_TO_TICKS(10));
        ret = pt_check(sel, mt);
        if(ret == 1 && status == MT_OPEN)
        {
            return 0;	// reach destination
        }
        else if(ret == -1 && status == MT_CLOSE)
        {
            return 0;	// reach destination
        }
        else 
        {
			// middle position
        }
		end = esp_timer_get_time();
		elapsed = (end-start)/1000;
		if(elapsed >= timeout)
		{
			return -1;	// timeout
		}
		if(sel == DC_MOTOR)
		{
            if(!get_dcmotor_run(mt))
            {
                return 0;
            }
		}
		else if(sel == STEP_MOTOR)
		{
            if(!get_stepmotor_run(mt))
            {
                return 0;
            }
		}
    } while(1);
	return ret;
}

static void start_diag_mode(int mode)
{
	int ret;
    mt_message_t msg = {0};
    
	switch(mode)
	{
		case DIAG_SENSOR_RF_MODE:
        	set_tof_sensor_enable(true);
        	break;
		case DIAG_PLATE_MODE:
            send_plate_msg(&msg, PLATE_CMD, 0, FORWARD, 0);
			break;
        case DIAG_SCPINOUT_MODE:
            ret = pt_check(DC_MOTOR, SCPINOUT_MOTOR);
            if(ret != 1)
            {
				send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
			}
        	break;
        case DIAG_SCPSPIN_MODE:
            ret = pt_check(DC_MOTOR, SCPINOUT_MOTOR);
            if(ret != 1)
            {
                send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
              	wait_diag_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000, MT_OPEN);
            }
            send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 360, FORWARD, 15000);
        	break;
        case DIAG_WASTE_MODE:
            ret = pt_check(DC_MOTOR, SCPINOUT_MOTOR);
            if(ret != 1)
            {
                send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
              	wait_diag_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000, MT_OPEN);
            }
            ret = pt_check(STEP_MOTOR, WASTE_COVER_MOTOR);
            if(ret != 1)
            {
            	send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, FORWARD, 15000);
			}
        	break;
        default:
        	break;
	}
	diag_func = 0;
}

static void finish_diag_mode(int mode)
{
	int ret;
    mt_message_t msg = {0};
    
	switch(mode)
	{
		case DIAG_SENSOR_RF_MODE:
        	set_tof_sensor_enable(false);
        	break;
		case DIAG_PLATE_MODE:
			send_plate_msg(&msg, PLATE_CMD, 0, STOP, 0);
			break;
        case DIAG_SCPINOUT_MODE:
            ret = pt_check(DC_MOTOR, SCPINOUT_MOTOR);
            if(ret != 1)
            {
                send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
              	wait_diag_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000, MT_OPEN);
            }
        	break;
        case DIAG_SCPSPIN_MODE:
#if 0        
            ESP_LOGI(TAG, "find scoop spin origin");
            ret = get_pt_status();
            if(!((ret&PT_BIT_SCP_SPIN_ST) && (ret&PT_BIT_SCP_SPIN_ED)))
            {
                send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 360, FORWARD, 15000);
                do{
                    vTaskDelay(pdMS_TO_TICKS(MOTOR_CMD_TRANSFER_DELAY));
                    ret = get_pt_status();
                    if((ret&PT_BIT_SCP_SPIN_ST) && (ret&PT_BIT_SCP_SPIN_ED))
                    {
                        // check origin angle
                        send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 0, STOP, 0);
                        ESP_LOGI(TAG, "%s found origin angle %d ", __func__, (get_scpspin_cnt()*360)/10200);
                        break;
                    }
                    if(!get_stepmotor_run(SCPSPIN_MOTOR))
                    {
                        break;
                    }
                } while(1);
            }
#else
			send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 0, STOP, 0);
#endif
        	
        	break;
        case DIAG_WASTE_MODE:
	        ret = pt_check(STEP_MOTOR, WASTE_COVER_MOTOR);
	        if(ret != -1)
	        {
                send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, REVERSE, 15000);
                wait_diag_operation(STEP_MOTOR, WASTE_COVER_MOTOR, 15000, MT_CLOSE);
                send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
	        }
	        
			ESP_LOGI(TAG, "find scoop spin origin");
			ret = get_pt_status();
			if(!((ret&PT_BIT_SCP_SPIN_ST) && (ret&PT_BIT_SCP_SPIN_ED)))
			{
			    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 360, FORWARD, 15000);
			    do{
			        vTaskDelay(pdMS_TO_TICKS(MOTOR_CMD_TRANSFER_DELAY));
			        ret = get_pt_status();
			        if((ret&PT_BIT_SCP_SPIN_ST) && (ret&PT_BIT_SCP_SPIN_ED))
			        {
			            // check origin angle
			            send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 0, STOP, 0);
			            ESP_LOGI(TAG, "%s found origin angle %d ", __func__, (get_scpspin_cnt()*360)/10200);
			            break;
			        }
			        if(!get_stepmotor_run(SCPSPIN_MOTOR))
			        {
			            break;
			        }
			    } while(1);
			}
			
            send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 90, REVERSE, 10000);
            do{
                vTaskDelay(pdMS_TO_TICKS(MOTOR_CMD_TRANSFER_DELAY));
                if(!get_stepmotor_run(SCPSPIN_MOTOR))
                {
                    break;
                }
            } while(1);

            send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 10000);
            wait_diag_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000, MT_CLOSE);
            send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
        
            send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 20, REVERSE, 5000);
            do{
                vTaskDelay(pdMS_TO_TICKS(MOTOR_CMD_TRANSFER_DELAY));
                if(!get_stepmotor_run(SCPSPIN_MOTOR))
                {
                    break;
                }
            } while(1);
        	break;
        default:
        	break;
	}
	diag_func = 0;
}

// 진단 모드에서 선택된 하드웨어(mode)의 다음 테스트 동작을 수행하는 함수
static void next_diag_func(int mode)
{
	int ret;
    mt_message_t msg = {0};
    
	switch(mode)
	{
		/* ===== 1. 내부 플레이트(Plate) 구동 테스트 ===== */
		case DIAG_PLATE_MODE:	
			if(diag_func >= 2)
			{
				diag_func = 0;
			}
			if(diag_func == 0)
			{
                send_plate_msg(&msg, PLATE_CMD, 0, FORWARD, 0);
			}
			else if(diag_func == 1)
			{
				send_plate_msg(&msg, PLATE_CMD, 0, REVERSE, 0);
			}
			break;

		/* ===== 2. 스쿱 전후진(Scoop In/Out) 구동 테스트 ===== */
        case DIAG_SCPINOUT_MODE:
			if(diag_func >= 2)
			{
				diag_func = 0;
			}
			if(diag_func == 0)
			{
				ret = pt_check(DC_MOTOR, SCPINOUT_MOTOR);
				if(ret != 1)
				{
                	send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
				}
			}
			else if(diag_func == 1)
			{
				ret = pt_check(DC_MOTOR, SCPINOUT_MOTOR);
				if(ret != -1)
				{
                	send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 10000);
                }
			}
        	break;

        /* ===== 3. 스쿱 회전(Scoop Spin) 구동 테스트 ===== */	
        case DIAG_SCPSPIN_MODE:
	        if(diag_func >= 2)
	        {
	            diag_func = 0;
	        }
			if(diag_func == 0)
			{
                send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 360, FORWARD, 15000);
			}
			else if(diag_func == 1)
			{
				send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 360, REVERSE, 15000);
			}
        	break;

		/* ===== 4. 폐기물 통 덮개(Waste Cover) 구동 테스트 ===== */        	
        case DIAG_WASTE_MODE:
	        if(diag_func >= 2)
	        {
	            diag_func = 0;
	        }
			if(diag_func == 0)
			{
				ret = pt_check(STEP_MOTOR, WASTE_COVER_MOTOR);
				if(ret != 1)
				{
                	send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, FORWARD, 15000);
				}
			}
			else if(diag_func == 1)
			{
				ret = pt_check(STEP_MOTOR, WASTE_COVER_MOTOR);
				if(ret != -1)
				{
                	send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, REVERSE, 15000);
                }
			}
        	break;
        default:
        	break;
	}
}

static void diag_check_pt(int mode)
{
	int ret;
    mt_message_t msg = {0};
    
	switch(mode)
	{
		case DIAG_PLATE_MODE:	
			break;
        case DIAG_SCPINOUT_MODE:
/*			ret = pt_check(DC_MOTOR, SCPINOUT_MOTOR);
			if(ret == 1 || ret == -1)
			{
            	send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
			}*/
        	break;
        case DIAG_SCPSPIN_MODE:
        	break;
        case DIAG_WASTE_MODE:
/*			ret = pt_check(STEP_MOTOR, WASTE_COVER_MOTOR);
			if(ret == 1 || ret == -1)
			{
            	send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
			} */
        	break;
        default:
        	break;
	}
}

// TBD : ble ping test 
static void diag_sensor_rf(int mode)
{
    float m_weight, w_weight;
	switch(mode)
	{
		case DIAG_SENSOR_RF_MODE:	
            w_weight = get_weight(LOADCELL_WASTE);
            m_weight = get_weight(LOADCELL_MAIN);
//			ESP_LOGI(TAG, "main: %.1fg waste: %.1fg", m_weight, w_weight);
            printf("m %.1fg w %.1fg\n", m_weight, w_weight);
			vTaskDelay(pdMS_TO_TICKS(500));
			break;
        default:
        	break;
	}
}

// do diagnostic mode 진단모드 
static void do_diag_mode(int mode)
{
	ESP_LOGI(TAG, "%s mode %d +", __func__, mode);
	start_diag_mode(mode);
	while(1)
	{
		if(diag_next_step_f)
		{
			diag_next_step_f = false;
			break;
		}

		if(diag_next_func_f)
		{
			diag_func++;
			next_diag_func(mode);
			diag_next_func_f = false;
		}
		diag_check_pt(mode);
		diag_sensor_rf(mode);
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	finish_diag_mode(mode);
	
}

void diag_process_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);
	
    message_t msg;
	send_led_cmd_msg(&msg, LED_QCMODE_CMD);

#ifdef FEATURE_MAIN_COVER	
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, FORWARD, 10000);
    wait_diag_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_OPEN);
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
#endif
	do{
        if(!get_keyclean_status())
        {
        	ESP_LOGI(TAG, "%s clean key released !!", __func__);
        	vTaskDelay(pdMS_TO_TICKS(100));	// key flush need time
        	break;
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}while(1);

	// DIAG_SENSOR_RF_MODE
    diag_next_step_f = false;
    diag_next_func_f = false;

	do_diag_mode(DIAG_SENSOR_RF_MODE);	
	do_diag_mode(DIAG_PLATE_MODE);	
	do_diag_mode(DIAG_SCPINOUT_MODE);	
	do_diag_mode(DIAG_SCPSPIN_MODE);	
	do_diag_mode(DIAG_WASTE_MODE);	

#ifdef FEATURE_MAIN_COVER
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, REVERSE, 10000);
    wait_diag_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_CLOSE);
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
#endif
    system_reset(REASON_DIAG);
    vTaskDelete(NULL);
}

void diag_cmd_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);

    while (1) {
        message_t msg;
        if (xQueueReceive(diag_cmd_msg, &msg, portMAX_DELAY) == pdPASS) {
//          ESP_LOGI(TAG, "[Receiver %ld] transfer complete -> cmd %d ", msg.task_id, msg.cmd);
            switch((int)(msg.cmd))
            {
                case DIAG_IDLE_CMD:
                	break;
                	
                case DIAG_NEXT_STEP_CMD:
                	diag_next_step_f = true;
                	break;
                case DIAG_NEXT_FUNC_CMD:
                	diag_next_func_f = true;
                    break;
                
                default:
                	break;
            }
        }
    }
}

void diag_init(void)
{
	ESP_LOGI(TAG, "%s", __func__);

    diag_cmd_msg = xQueueCreate(10, sizeof(message_t));

    xTaskCreate(diag_cmd_task, "diag_cmd_task", 3072, NULL, 10, NULL);
    xTaskCreate(diag_process_task, "diag_process_task", 3072, NULL, 10, NULL);
}
