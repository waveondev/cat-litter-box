#include "main.h"
#include <math.h>

static const char *TAG = "UI";

#define UI_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

// 전역 변수 실체 정의
volatile bool g_is_system_ready = false;

extern void check_boot_origin_recovery(void);

static bool pairing_task_run = false;
static bool factory_task_run = false;
static bool tare_task_run = false;
static bool ui_manage_f = false;

static bool clean_task_run = false;
static bool manage_finish_task_run = false;
static bool manage_start_task_run = false;

static QueueHandle_t ui_cmd_msg = NULL;

void send_ui_cmd_msg(void *message, uint32_t cmd)
{
	message_t *msg = (message_t *)message;
    msg->cmd = cmd;
    BaseType_t status = xQueueSend(ui_cmd_msg, msg, pdMS_TO_TICKS(100));
    if (status != pdPASS) 
    {
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
	do_manage_finish(arg);
	ui_manage_f = false;
	manage_finish_task_run = false;
	vTaskDelete(NULL);
}

void ui_manage_start_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);
	manage_start_task_run = true;
	do_manage_start(arg);
	manage_start_task_run = false;
	vTaskDelete(NULL);
}

void ui_tarezero_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);
	tare_task_run = true;
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
#ifdef FEATURE_AWS_IOT
	Wifi_Disconnect();
	wifi_scan_start();
#endif
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
    
    send_led_cmd_msg(&led_msg, LED_INITOK_CMD);

    ESP_ERROR_CHECK(nvs_flash_erase());
    nvs_flash_init();

	system_reset(REASON_FACTORY_RESTORE);
	
	factory_task_run = false;
	vTaskDelete(NULL);
}

static bool check_run_flag(bool manage)
{
	if(!clean_task_run 
			&& !pairing_task_run 
			&& !factory_task_run 
			&& !tare_task_run 
			&& !manage_finish_task_run 
			&& !manage_start_task_run)
	{
		if(manage)
		{
			if(ui_manage_f) return true;		
			else return false;
		}
		else
		{
			if(!ui_manage_f) return true;
			else return false;
		}
	}
	return false;
}

bool check_task_runnable(int task)
{
	bool ret = false;
	switch(task)
	{
        case UI_CLEAN_CMD:
        	if(check_run_flag(false)) return true;
        	break;
        case UI_MANAGE_FINISH_CMD:
        	if(check_run_flag(true)) return true;
        	break;
        case UI_TARE_ZERO_CMD:
        	if(check_run_flag(false)) return true;
        	break;
        case UI_MANAGE_START_CMD:
        	if(check_run_flag(false)) return true;
        	break;
        case UI_PAIRING_CMD:
        	if(check_run_flag(false)) return true;
        	break;
        case UI_FACTORY_CMD:
        	if(check_run_flag(false)) return true;
        	break;
        default:
        	break;
	}
	return ret;
}

void ui_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);

	while(1)
	{
        message_t msg;
        if (xQueueReceive(ui_cmd_msg, &msg, portMAX_DELAY) == pdPASS) 
        {
	        switch((int)(msg.cmd))
	        {
				case UI_CLEAN_CMD:
                	ESP_LOGI(TAG, "UI_CLEAN_CMD");
                	if(check_task_runnable((int)msg.cmd))
                	{
						xTaskCreate(ui_clean_task, "ui_clean_task", 4096, NULL, 10, NULL);
                	}
					break;
				case UI_MANAGE_FINISH_CMD:
                	ESP_LOGI(TAG, "UI_MANAGE_FINISH_CMD");
                	if(check_task_runnable((int)msg.cmd))
                	{
                		if(ui_manage_f)
                		{
							xTaskCreate(ui_manage_finish_task, "ui_manage_finish_task", 4096, NULL, 10, NULL);
						}
                	}
					break;
				case UI_TARE_ZERO_CMD:
                	ESP_LOGI(TAG, "UI_TARE_ZERO_CMD");
                	if(check_task_runnable((int)msg.cmd))
                	{
						xTaskCreate(ui_tarezero_task, "ui_tarezero_task", 4096, NULL, 10, NULL);
                	}
					break;
				case UI_MANAGE_START_CMD:
                	ESP_LOGI(TAG, "UI_MANAGE_START_CMD");
                	if(check_task_runnable((int)msg.cmd))
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
                	if(check_task_runnable((int)msg.cmd))
                	{
						xTaskCreate(ui_pairing_task, "ui_pairing_task", 4096, NULL, 10, NULL);
                	}
					break;
				case UI_FACTORY_CMD:
                	ESP_LOGI(TAG, "UI_FACTORY_CMD");
                	if(check_task_runnable((int)msg.cmd))
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

static bool check_operating(void)
{
    if(clean_task_run
	    || pairing_task_run 
	    || factory_task_run 
	    || tare_task_run 
	    || manage_finish_task_run 
	    || manage_start_task_run 
	    || ui_manage_f
	    || get_status_diag())
	    {
			return true;
	    }
	return false;
}

static void print_boot_photo_sensor_status(void)
{
    int current_pt = get_pt_status();

    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "[BOOT PHOTO SENSOR STATUS] RAW: 0x%04X (%d)", current_pt, current_pt);

    struct {
        int mask;
        const char* name;
    } sensor_list[] = {
        { PT_BIT_SCP_SPIN_ST, "SCP_SPIN_ST (스쿱 회전 시작)" },
        { PT_BIT_SCP_OUT,     "SCP_OUT     (스쿱 전진 끝)" },
        { PT_BIT_WASTE_CLOSE, "WASTE_CLOSE (배변통 닫힘)" },
        { PT_BIT_MCOVER_OPEN, "MCOVER_OPEN (메인커버 열림)" },
        { PT_BIT_SCP_SPIN_ED, "SCP_SPIN_ED (스쿱 회전 끝)" },
        { PT_BIT_SCP_IN,      "SCP_IN      (스쿱 후진 끝)" },
        { PT_BIT_WASTE_OPEN,  "WASTE_OPEN  (배변통 열림)" },
        { PT_BIT_REED_SW,     "REED_SW     (리드 스위치)" },
        { PT_BIT_MCOVER_CLOSE,"MCOVER_CLOSE(메인커버 닫힘)" }
    };

    int list_size = sizeof(sensor_list) / sizeof(sensor_list[0]);
    for (int i = 0; i < list_size; i++) {
        bool is_active = (current_pt & sensor_list[i].mask) != 0;
        ESP_LOGI(TAG, "  ├─ %-26s : %d (%s)", 
                 sensor_list[i].name, 
                 is_active ? 1 : 0, 
                 is_active ? "DETECTED / HIGH" : "RELEASED / LOW");
    }
    ESP_LOGI(TAG, "=================================================");

    // 준비 완료 후 오리진 상태 검사 및 자동 복구 수행
    check_boot_origin_recovery();
}

void proximity_task(void *arg)
{
	ESP_LOGI(TAG, "%s +", __func__);
	bool cat_enter_f = false;
	int cat_enter_cnt = 0;
	int cat_use_cnt = 0;
	int mode;
	int harden_tm = 0;
	double prev_wg = 0.0f;
	double cat_wg = 0.0f;
	double gap = 0.0f;
	app_config_t *app = get_app_config();
    message_t lmsg = {0};
	
	cat_wg = (double)(app->CAT_ENTRY_MIN_WEIGHT);
	harden_tm = (int)app->CLUMPING_WAIT_MIN;
	ESP_LOGI(TAG, "CAT_ENTRY_MIN_WEIGHT %.1f CLUMPING_WAIT_MIN %d ", cat_wg, harden_tm);
	
	mode = PROXI_MODE_IDLE;
	while(1)
	{
		if(check_operating())
		{
			cat_enter_f = false;
			cat_enter_cnt = 0;
			cat_use_cnt = 0;
			mode = PROXI_MODE_IDLE;
		}
		else
		{
			switch(mode)
			{
	            case PROXI_MODE_IDLE:
	            	if(get_weight(LOADCELL_MAIN) > cat_wg)
	            	{
	            		if(!cat_enter_f)
	            		{
	            			cat_enter_f = true;
	            			cat_enter_cnt = 0;
	            			ESP_LOGI(TAG, "cat enter");
						}
						else
						{
							cat_enter_cnt++;
                            if(cat_enter_cnt > 10)	// 5 sec
                            {
                                cat_enter_f = false;
                                cat_enter_cnt = 0;
                                cat_use_cnt = 0;
                                mode = PROXI_MODE_USE;
                              	ESP_LOGI(TAG, "to PROXI_MODE_USE prev_wg %.1fg" , prev_wg);
                                send_led_cmd_msg(&lmsg, LED_INUSE_CMD);
                            }
						}
	            	}
	            	else
	            	{
	            		if(cat_enter_f)
	            		{
							cat_enter_f = false;
	            			cat_enter_cnt = 0;
	            			ESP_LOGI(TAG, "cat moving 1");
	            		}
	            		else
	            		{
	            			cat_enter_cnt++;
                            if(cat_enter_cnt > 10)
                            {
                            	if(cat_enter_cnt == 11)
                            	{
                                    ESP_LOGI(TAG, "cat moving 2");
                                    prev_wg = (double)get_weight(LOADCELL_MAIN);
                            	}
                            	else
								{
									cat_enter_cnt = 12;
									gap = (double)get_weight(LOADCELL_MAIN);

									if (prev_wg < 100.0) {
										prev_wg = gap;
									}

									if(fabs(gap - prev_wg) < 3.0)
									{
										prev_wg = gap;

										if (!g_is_system_ready) {
											send_led_cmd_msg(&lmsg, LED_IDLE_CMD);
											g_is_system_ready = true;

											ESP_LOGI(TAG, "=================================================");
											ESP_LOGI(TAG, " [CAT FSM STATE CHANGED] -> State: 1 (CAT_STATE_IDLE)");
											ESP_LOGI(TAG, " [SYSTEM READY] White LED On & 3 Buttons Enabled!");
											ESP_LOGI(TAG, "=================================================");

											print_boot_photo_sensor_status();
										}
									}
									else
									{
										prev_wg = gap;
									}
								}
                            }
						}
	            	}
	            	break;
	            case PROXI_MODE_USE:
	            	if(get_weight(LOADCELL_MAIN) > cat_wg)
	            	{
						cat_use_cnt++;
						cat_enter_cnt = 0;
	            	}
	            	else
	            	{
						cat_enter_cnt++;
						if(cat_enter_cnt > 10)	// 5 sec chattering
						{
							mode = PROXI_MODE_ESCAPE;

							ESP_LOGI(TAG, "to PROXI_MODE_ESCAPE !!");
						}
						ESP_LOGI(TAG, "cat moving !!");
	            	}
	            	break;
	            case PROXI_MODE_ESCAPE:
	            	ESP_LOGI(TAG, "stay during %.1f sec prev_wg %.1fg cur_wg %.1fg delta %.1fg !!"
	            		, ((float)cat_use_cnt)/2.0f, prev_wg, get_weight(LOADCELL_MAIN), get_weight(LOADCELL_MAIN)-prev_wg);
	            	cat_enter_cnt = 0;
                    mode = PROXI_MODE_HARDEN;

                    send_led_cmd_msg(&lmsg, LED_IDLE_CMD);
	            	break;
	            case PROXI_MODE_HARDEN:
	            	cat_enter_cnt++;
	            	if(cat_enter_cnt%10 == 0)
	            	{
                      	ESP_LOGI(TAG, "%s wait hardening ...", __func__);
	            	}
					if(cat_enter_cnt > 20)    // 10 sec, test
	            	{
                        cat_enter_cnt = 0;
                        cat_enter_f = false;
                        mode = PROXI_MODE_IDLE;
                        ESP_LOGI(TAG, "to PROXI_MODE_IDLE 10 minute harden finished !!");
						xTaskCreate(ui_clean_task, "ui_clean_task", 4096, NULL, 10, NULL);
	            	}
	            	break;
				default:
					break;
			}
		}
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void ui_init(void)
{
	ESP_LOGI(TAG, "%s", __func__);	
	g_is_system_ready = false;
	ui_cmd_msg = xQueueCreate(10, sizeof(message_t));
    xTaskCreate(ui_task, "ui_task", UI_TASK_STACK_SIZE, NULL, 10, NULL);
    xTaskCreate(proximity_task, "proximity_task", 3072, NULL, 10, NULL);
}