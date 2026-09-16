#include "main.h"

static const char *TAG = "MOTOR";

static bool spin_mt_always_on = false;

static QueueHandle_t motor_msg = NULL;

// [추가] 긴급 정지 플래그 및 매크로
volatile bool emergency_stop_flag = false;
#define CHECK_ESTOP() if(emergency_stop_flag) { ESP_LOGW(TAG, "Emergency Stop Triggered! Aborting scenario."); goto estop_exit; }

int pt_check(int sel, int mt)
{
    int ret = 0;
    if(sel == STEP_MOTOR)
    {
        if(mt == WASTE_COVER_MOTOR)
        {
            if(get_pt_status()&PT_BIT_WASTE_CLOSE) return -1;
            else if(get_pt_status()&PT_BIT_WASTE_OPEN) return 1;
        }
    }
    else if(sel == DC_MOTOR)
    {
        if(mt == MAIN_COVER_MOTOR)
        {
            if(get_pt_status()&PT_BIT_MCOVER_CLOSE) return -1;
            else if(get_pt_status()&PT_BIT_MCOVER_OPEN) return 1;
        }
        if(mt == SCPINOUT_MOTOR)
        {
            if(get_pt_status()&PT_BIT_SCP_IN) return -1;
            else if(get_pt_status()&PT_BIT_SCP_OUT) return 1;
        }
    }
    return ret;
}

static int wait_motor_operation(int sel, int mt, int timeout, int status)
{
    vTaskDelay(pdMS_TO_TICKS(40));  
    
    int64_t start, end, elapsed;
    start = esp_timer_get_time();
    int ret = 0;
    do{
        // 긴급 정지 감지 시 즉시 탈출        
        if (emergency_stop_flag) return -1;
    
        vTaskDelay(pdMS_TO_TICKS(10));
        ret = pt_check(sel, mt);
        if(ret == 1 && status == MT_OPEN) return 0;
        else if(ret == -1 && status == MT_CLOSE) return 0;
        
        end = esp_timer_get_time();
        elapsed = (end-start)/1000;
        if(elapsed >= timeout) return -1;   
        
        if(sel == DC_MOTOR) {
            if(!get_dcmotor_run(mt)) return 0;
        } else if(sel == STEP_MOTOR) {
            if(!get_stepmotor_run(mt)) return 0;
        }
    } while(1);
    return ret;
}

int motor_main_cover_test(int dir)
{
#ifdef FEATURE_MAIN_COVER   
    mt_message_t msg = {0};
    send_motor_msg(&msg, EMERGENCY_RESET_CMD, 0, 0, 0);
    if(dir == 1) {
        send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, FORWARD, 10000);
        wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_OPEN);
        send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
    } else if(dir == -1) {
        send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, REVERSE, 10000);
        wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_CLOSE);
        send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
    } else {
        send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
    }
#endif
    return 0;
}

int motor_waste_cover_test(int dir)
{
    mt_message_t msg = {0};
    send_motor_msg(&msg, EMERGENCY_RESET_CMD, 0, 0, 0);
    if(dir == 1) {
        send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, FORWARD, 15000);
        wait_motor_operation(STEP_MOTOR, WASTE_COVER_MOTOR, 15000, MT_OPEN);
    } else if(dir == -1) {
        send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, REVERSE, 15000);
        wait_motor_operation(STEP_MOTOR, WASTE_COVER_MOTOR, 15000, MT_CLOSE);
    } else {
        send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
    }
    return 0;
}

bool check_spin_mt_on(void)
{
    return spin_mt_always_on;
}

int wait_duration(int ms)
{
    int cnt;
    if(ms < 100) return -1;
    
    cnt = ms;
    cnt /= 100;
    if((ms%100)!= 0) cnt++;
    
    do{
        // 긴급 정지 감지 시 즉시 탈출        
        if (emergency_stop_flag) return -1;
        vTaskDelay(pdMS_TO_TICKS(100));
    }while(cnt-- > 0);
    
    return 0;
}

int do_clean(void *arg)
{
    mt_message_t msg = {0};
    message_t led_msg = {0};
    msg.task_id = (uint32_t)arg;
    led_msg.task_id = (uint32_t)arg;

    send_led_cmd_msg(&led_msg, LED_CLEANING_CMD);
    set_tof_sensor_enable(true);
    send_motor_msg(&msg, EMERGENCY_RESET_CMD, 0, 0, 0);
    send_plate_msg(&msg, PLATE_CMD, 0, FORWARD, 0);
    
#ifdef FEATURE_MAIN_COVER   
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, FORWARD, 10000);
    wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_OPEN);
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
#endif

    spin_mt_always_on = true;

    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 180, FORWARD, 3000);
    wait_duration(500);
    CHECK_ESTOP(); 
    
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
    wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000, MT_OPEN);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
    CHECK_ESTOP(); 

#ifdef FEATURE_CLEAN_QUICK_DEMO
    wait_duration(5*1000);  
#else
    wait_duration(90*1000); 
#endif
    CHECK_ESTOP();
    
    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 70, REVERSE, 10000);
    send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, FORWARD, 15000);

#ifdef FEATURE_SHAKE_SCOOP
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 5000, MT_MIDDLE);
    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 30, REVERSE, 1000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 2000, MT_MIDDLE);
    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 30, FORWARD, 1000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 2000, MT_MIDDLE);
    
    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 30, REVERSE, 1000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 2000, MT_MIDDLE);
    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 30, FORWARD, 1000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 2000, MT_MIDDLE);
    
    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 30, REVERSE, 1000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 2000, MT_MIDDLE);
    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 30, FORWARD, 1000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 2000, MT_MIDDLE);

    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 30, REVERSE, 1000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 2000, MT_MIDDLE);
    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 30, FORWARD, 1000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 2000, MT_MIDDLE);
#endif
    
    wait_motor_operation(STEP_MOTOR, WASTE_COVER_MOTOR, 10000, MT_OPEN);
    send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);

    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 10000);
    wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000, MT_CLOSE);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 130, REVERSE, 10000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 10000, MT_MIDDLE);

    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 40, FORWARD, 1000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 2000, MT_MIDDLE);

    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
    wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000, MT_OPEN);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

    send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, REVERSE, 15000);

    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 150, FORWARD, 5000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 10000, MT_MIDDLE);

#ifdef FEATURE_CLEAN_QUICK_DEMO
    vTaskDelay(pdMS_TO_TICKS(5000));
#else
    wait_duration(60*1000);     
#endif
    CHECK_ESTOP();

    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 150+80, REVERSE, 5000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 10000, MT_MIDDLE);

    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 80, FORWARD, 5000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 3000, MT_MIDDLE);

    spin_mt_always_on = false;

    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 10000);
    wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000, MT_CLOSE);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 20, REVERSE, 5000);
    wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 5000, MT_MIDDLE);

#ifdef FEATURE_MAIN_COVER
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, REVERSE, 10000);
    wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_CLOSE);
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
#endif

    send_plate_msg(&msg, PLATE_CMD, 0, STOP, 0);
    set_tof_sensor_enable(false);

    send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
    return 0;

estop_exit:    
    spin_mt_always_on = false;    
    set_tof_sensor_enable(false);    
    send_led_cmd_msg(&led_msg, LED_ERROR_CMD); 
    return -1;
}

int do_manage_start(void *arg)
{
    message_t led_msg = {0};
    mt_message_t msg = {0};
    msg.task_id = (uint32_t)arg;
    led_msg.task_id = (uint32_t)arg;

    send_led_cmd_msg(&led_msg, LED_MANAGE_CMD);
    set_tof_sensor_enable(true);
    send_motor_msg(&msg, EMERGENCY_RESET_CMD, 0, 0, 0);

#ifdef FEATURE_MAIN_COVER   
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, FORWARD, 10000);
    wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_OPEN);
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
#endif

    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
    wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000, MT_OPEN);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
    set_tof_sensor_enable(false);

    return 0;
}

int do_manage_finish(void *arg)
{
    mt_message_t msg = {0};
    message_t led_msg = {0};
    msg.task_id = (uint32_t)arg;
    led_msg.task_id = (uint32_t)arg;

    set_tof_sensor_enable(true);
    send_motor_msg(&msg, EMERGENCY_RESET_CMD, 0, 0, 0);
    
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 10000);
    wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000, MT_CLOSE);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

#ifdef FEATURE_MAIN_COVER   
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, REVERSE, 10000);
    wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_CLOSE);
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
#endif
    set_tof_sensor_enable(false);
    send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
    return 0;
}

void send_motor_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout)
{
    mt_message_t *msg = (mt_message_t *)message;
    msg->cmd = cmd;
    msg->angle = angle;
    msg->direction = dir;
    msg->timeout = timeout;
    xQueueSend(motor_msg, msg, pdMS_TO_TICKS(100));
}

void motor_cmd_task(void *arg) {
    while (1) {
        mt_message_t msg = {0};
        if (xQueueReceive(motor_msg, &msg, portMAX_DELAY) == pdPASS) {
            switch((int)(msg.cmd))
            {
                case EMERGENCY_RESET_CMD:
                    break;
                default:
                break;
            }
        }
    }
    vTaskDelete(NULL);
}

void motor_init(void)
{
    motor_msg = xQueueCreate(10, sizeof(mt_message_t));
    xTaskCreate(motor_cmd_task, "motor_cmd_task", 4096, NULL, 5, NULL);
}

void set_emergency_stop(void)
{
    emergency_stop_flag = true;
    mt_message_t msg = {0};
    
    // 5개의 모든 모터에 즉각 STOP 명령 전송
    send_plate_msg(&msg, PLATE_CMD, 0, STOP, 0);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
    send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 0, STOP, 0);
}

void clear_emergency_stop(void)
{
    emergency_stop_flag = false;
}
