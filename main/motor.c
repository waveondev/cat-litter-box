#include "main.h"

static const char *TAG = "MOTOR";
#define MOTOR_CMD_TRANSFER_DELAY	(50)
int spin_angle[10] = {0, 34, 87, 155, 167, 193, 205, 282, 323, 355};
static int pt_check(int sel, int mt)
{
	int ret = 0;
	if(sel == STEP_MOTOR)
	{
		if(mt == MAIN_COVER_MOTOR)
		{
			if((get_pt_status()&PT_BIT_MCOVER_CLOSE) || (get_pt_status()&PT_BIT_MCOVER_OPEN))
			{
				return 1;
			}
		}
#if 0			// not implemented yet, for mechanic issue
		else if(mt == WASTE_COVER_MOTOR)
		{
			if((get_pt_status()&PT_BIT_WASTE_OPEN) || (get_pt_status()&PT_BIT_WASTE_CLOSE))
			{
				return 1;
			}
		}
		else if(mt == SCPSPIN_MOTOR)
		{
			if((get_pt_status()&PT_BIT_SCP_SPIN_ED) || (get_pt_status()&PT_BIT_SCP_SPIN_ST))
			{
				return 1;
			}
		}
#endif
	}
	else if(sel == DC_MOTOR)
	{
		if(mt == SCPINOUT_MOTOR)
		{
			if((get_pt_status()&PT_BIT_SCP_OUT) || (get_pt_status()&PT_BIT_SCP_IN))
			{
				return 1;
			}
		}
	}
	return ret;
}

static int wait_motor_operation(int sel, int mt, int timeout)
{
	int ret = 0;
	int i;
	
    int64_t start, end, elapsed;
    start = esp_timer_get_time();
    i = 0;
    do{
        vTaskDelay(pdMS_TO_TICKS(10));
        i++;
        if(i>100)
        {
	        ret = pt_check(sel, mt);
	        if(ret == 1)
	        {
	        	ESP_LOGI(TAG, "pt matched!");
	            return 0;	// reach destination
	        }
        }
        else
        {
//        	ESP_LOGI(TAG, "wait 1 sec");
        }
		end = esp_timer_get_time();
		elapsed = (end-start)/1000;
		if(elapsed >= timeout)
		{
            ESP_LOGI(TAG, "%s timeout, sel %d mt %d timeout %d ", __func__, sel, mt, timeout);
			return -1;	// timeout
		}
		if(sel == DC_MOTOR)
		{
            if(!get_dcmotor_run(mt))
            {
            	ESP_LOGI(TAG, "dc motor %d finished to run!", mt);
                return 0;
            }
		}
		else if(sel == STEP_MOTOR)
		{
            if(!get_stepmotor_run(mt))
            {
            	ESP_LOGI(TAG, "step motor %d finished to run!", mt);
                return 0;
            }
		}
    } while(1);
	return ret;
}

int motor_calibration(void *arg)
{
	int pt;
//	int ret;
    mt_message_t msg = {0};
	int main_cover, waste_cover, scp_inout;
    msg.task_id = (uint32_t)arg;

	
    ESP_LOGI(TAG, "pt status check");
    pt = get_pt_status();
    
    // main cover check
	if(pt&PT_BIT_MCOVER_OPEN)
	{
		main_cover = PT_END;
	}
	else if(pt&PT_BIT_MCOVER_CLOSE)
	{
		main_cover = PT_START;
	}
	else
	{
		main_cover = PT_MIDDLE;
	}
	// waste cover check
	if(pt&PT_BIT_WASTE_OPEN)
	{
		waste_cover = PT_END;
	}
	else if(pt&PT_BIT_WASTE_CLOSE)
	{
		waste_cover = PT_START;
	}
	else
	{
		waste_cover = PT_MIDDLE;
	}
	// scp inout check
	if(pt&PT_BIT_SCP_OUT)
	{
	    scp_inout = PT_END;
	}
	else if(pt&PT_BIT_SCP_IN)
	{
	    scp_inout = PT_START;
	}
	else
	{
	    scp_inout = PT_MIDDLE;
	}
#if 0
	// main cover open
	if(main_cover == PT_MIDDLE || main_cover == PT_START)
	{
		send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, FORWARD, 5000);
        do{
            vTaskDelay(pdMS_TO_TICKS(MOTOR_CMD_TRANSFER_DELAY));
            pt = get_pt_status();
            if(pt&PT_BIT_MCOVER_OPEN)
            {
            	send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
            	break;
            }
            if(!get_stepmotor_run(MAIN_COVER_MOTOR))
            {
                break;
            }
        } while(1);
	}
#endif
	if(waste_cover == PT_START)
	{
		if(scp_inout != PT_END)
		{
	        send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
	        do{
	            vTaskDelay(pdMS_TO_TICKS(MOTOR_CMD_TRANSFER_DELAY));
	            pt = get_pt_status();
	            if(pt&PT_BIT_SCP_OUT)
	            {
	            	ESP_LOGI(TAG, "scp inout pt end");
	                send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
	                break;
	            }
	            if(!get_dcmotor_run(SCPINOUT_MOTOR))
	            {
	            	ESP_LOGI(TAG, "scp inout run false");
	                break;
	            }
			} while(1);
		}
	}
	else if(waste_cover == PT_END)
	{
		if(scp_inout != PT_END)
		{
	        send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
	        do{
	            vTaskDelay(pdMS_TO_TICKS(MOTOR_CMD_TRANSFER_DELAY));
	            pt = get_pt_status();
	            if(pt&PT_BIT_SCP_OUT)
	            {
	                send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
	                break;
	            }
	            if(!get_dcmotor_run(SCPINOUT_MOTOR))
	            {
	                break;
	            }
			} while(1);
		}
        send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, REVERSE, 15000);
        do{
            vTaskDelay(pdMS_TO_TICKS(MOTOR_CMD_TRANSFER_DELAY));
            pt = get_pt_status();
            if(pt&PT_BIT_WASTE_CLOSE)
            {
                send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
                break;
            }
            if(!get_stepmotor_run(WASTE_COVER_MOTOR))
            {
                break;
            }
        } while(1);
	}
	else
	{
        send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, REVERSE, 15000);
        do{
            vTaskDelay(pdMS_TO_TICKS(MOTOR_CMD_TRANSFER_DELAY));
            pt = get_pt_status();
            if(pt&PT_BIT_WASTE_CLOSE)
            {
                send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
                break;
            }
            if(!get_stepmotor_run(WASTE_COVER_MOTOR))
            {
                break;
            }
        } while(1);
        send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
        do{
            vTaskDelay(pdMS_TO_TICKS(MOTOR_CMD_TRANSFER_DELAY));
            pt = get_pt_status();
            if(pt&PT_BIT_SCP_OUT)
            {
                send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
                break;
            }
            if(!get_dcmotor_run(SCPINOUT_MOTOR))
            {
                break;
            }
        } while(1);
	}

   	ESP_LOGI(TAG, "find scoop spin origin");
    if(!((pt&PT_BIT_SCP_SPIN_ST) && (pt&PT_BIT_SCP_SPIN_ED)))
	{
		send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 360, FORWARD, 15000);
	    do{
	        vTaskDelay(pdMS_TO_TICKS(MOTOR_CMD_TRANSFER_DELAY));
	        pt = get_pt_status();
			if((pt&PT_BIT_SCP_SPIN_ST) && (pt&PT_BIT_SCP_SPIN_ED))
			{
				// check origin angle
            	send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 0, STOP, 0);
				ESP_LOGI(TAG, "check origin angle %d ", (get_scpspin_cnt()*360)/10200);
				break;
			}
			if(!get_stepmotor_run(SCPSPIN_MOTOR))
			{
				break;
			}
/*
			if(get_pt_status()&PT_BIT_SCP_SPIN_ED)
			{
				ESP_LOGI(TAG, "ED matched %d dgree ", (get_scpspin_cnt()*360)/10200);
			}
			else if(get_pt_status()&PT_BIT_SCP_SPIN_ST)
			{
				ESP_LOGI(TAG, "ST matched %d degree ", (get_scpspin_cnt()*360)/10200);
			} */
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

    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 5000);
    do{
        vTaskDelay(pdMS_TO_TICKS(MOTOR_CMD_TRANSFER_DELAY));
        pt = get_pt_status();
        if(pt&PT_BIT_SCP_IN)
        {
            send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
            break;
        }
        if(!get_dcmotor_run(SCPINOUT_MOTOR))
        {
            break;
        }
    } while(1);

    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 20, REVERSE, 5000);
    do{
        vTaskDelay(pdMS_TO_TICKS(MOTOR_CMD_TRANSFER_DELAY));
        if(!get_stepmotor_run(SCPSPIN_MOTOR))
        {
            break;
        }
    } while(1);
#if 0
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, REVERSE, 5000);
    do{
        vTaskDelay(pdMS_TO_TICKS(MOTOR_CMD_TRANSFER_DELAY));
        pt = get_pt_status();
        if(pt&PT_BIT_MCOVER_CLOSE)
        {
            send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
            break;
        }
        if(!get_stepmotor_run(MAIN_COVER_MOTOR))
        {
            break;
        }
    } while(1);
#endif
    return 0;
}

int do_clean(void *arg)
{
	mt_message_t msg = {0};
	message_t led_msg;
	msg.task_id = (uint32_t)arg;
	led_msg.task_id = (uint32_t)arg;

	send_led_cmd_msg(&led_msg, LED_CLEANING_CMD);
#if 0

	send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 30, FORWARD, 5000);
	vTaskDelay(pdMS_TO_TICKS(5000));
#else
	// plate rotate start
	send_plate_msg(&msg, PLATE_CMD, 0, FORWARD, 0);
	
	// main cover open
//	send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, FORWARD, 10000);
//	wait_motor_operation(STEP_MOTOR, MAIN_COVER_MOTOR, 10000);
//	send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
	
	send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 180, FORWARD, 3000);
	vTaskDelay(pdMS_TO_TICKS(500));
	send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
	wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000);
	send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

	ESP_LOGI(TAG, "wait plate rotate .... ");
	vTaskDelay(pdMS_TO_TICKS(5000));

	send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 70, REVERSE, 10000);
	send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, FORWARD, 15000);
	
	wait_motor_operation(STEP_MOTOR, WASTE_COVER_MOTOR, 15000);
	send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);

	ESP_LOGI(TAG, "scp inout back ");
	send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 10000);
    wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000);
	send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

	send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 110, REVERSE, 10000);
	wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 10000);
//	send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 0, STOP, 0);

	send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 20, FORWARD, 1000);
	wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 1000);
//	send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 0, STOP, 0);

	send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
    wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

	send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, REVERSE, 15000);
    wait_motor_operation(STEP_MOTOR, WASTE_COVER_MOTOR, 5000);

	send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 140, REVERSE, 5000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 5000);

	ESP_LOGI(TAG, "flattening .... ");
	vTaskDelay(pdMS_TO_TICKS(5000));	// flattening

	send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 140, FORWARD, 5000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 5000);

	send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 10000);
    wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

	send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 20, REVERSE, 5000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 5000);

//	send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, REVERSE, 10000);
//	wait_motor_operation(STEP_MOTOR, MAIN_COVER_MOTOR, 10000);
//	send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);

	send_plate_msg(&msg, PLATE_CMD, 0, STOP, 0);
#endif
	send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
	
	return 0;
}

int do_manage_start(void *arg)
{
    mt_message_t msg = {0};
    msg.task_id = (uint32_t)arg;

    // main cover open
//  send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, FORWARD, 15000);
//  wait_motor_operation(STEP_MOTOR, MAIN_COVER_MOTOR, 5000);
//  send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);

    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
    wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
	return 0;
}

int do_manage_finish(void *arg)
{
    mt_message_t msg = {0};
    msg.task_id = (uint32_t)arg;


    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 10000);
    wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
    
    // main cover close
//  send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, REVERSE, 5000);
//  wait_motor_operation(STEP_MOTOR, MAIN_COVER_MOTOR, 5000);
//  send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
	return 0;
}

int pt_test(void *arg)
{
//	mt_message_t msg = {0};
//	msg.task_id = (uint32_t)arg;

	while(1)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
#if 0
	send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
	wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000);
	send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 10000);
	wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000);
#endif

#if 0
	send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, FORWARD, 15000);
	wait_motor_operation(STEP_MOTOR, WASTE_COVER_MOTOR, 15000);
	send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, REVERSE, 15000);
	wait_motor_operation(STEP_MOTOR, WASTE_COVER_MOTOR, 15000);
#endif
	return 0;
}
