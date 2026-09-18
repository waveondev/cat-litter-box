#include "main.h"

static const char *TAG = "MOTOR";

static bool spin_mt_always_on = false;
static QueueHandle_t motor_msg = NULL;

volatile bool emergency_stop_flag = false;

// CHECK_ESTOP 매크로: do_clean 함수 내에서 estop_exit 레이블로 탈출
#define CHECK_ESTOP() \
    do { \
        if (emergency_stop_flag) { \
            ESP_LOGW(TAG, "Emergency Stop Triggered! Aborting scenario."); \
            goto estop_exit; \
        } \
    } while(0)

int pt_check(int sel, int mt)
{
    int ret = 0;
    if(sel == STEP_MOTOR)
    {
        if(mt == WASTE_COVER_MOTOR)
        {
            if(get_pt_status() & PT_BIT_WASTE_CLOSE) return -1;
            else if(get_pt_status() & PT_BIT_WASTE_OPEN) return 1;
        }
    }
    else if(sel == DC_MOTOR)
    {
        if(mt == MAIN_COVER_MOTOR)
        {
            if(get_pt_status() & PT_BIT_MCOVER_CLOSE) return -1;
            else if(get_pt_status() & PT_BIT_MCOVER_OPEN) return 1;
        }
        if(mt == SCPINOUT_MOTOR)
        {
            if(get_pt_status() & PT_BIT_SCP_IN) return -1;
            else if(get_pt_status() & PT_BIT_SCP_OUT) return 1;
        }
    }
    return ret;
}

/**
 * @brief MAIN_COVER DC 모터 초기 고부하/정지마찰 극복용 강파워 소프트 구동 함수
 */
static void drive_main_cover_soft(int dir, uint32_t target_speed)
{
    mt_message_t msg = {0};

    // 1. Pre-Wakeup Wait: 오랜 정지 후 모터 드라이버 IC 전원 안정을 위해 50ms 대기
    vTaskDelay(pdMS_TO_TICKS(50));

    // 2. High-Power Kick Start: 극심한 정지 마찰을 뚫기 위해 100% 듀티를 300ms 동안 강력하게 인가
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, dir, 100);
    vTaskDelay(pdMS_TO_TICKS(300));

    // 3. High-Torque Ramp-Up: 70% 듀티부터 목표 속도까지 고토크 유지 및 가속
    uint32_t start_speed = 70;
    if (target_speed < start_speed) start_speed = target_speed;

    for (uint32_t duty = start_speed; duty <= target_speed; duty += 10) {
        send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, dir, duty);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

/**
 * @brief 모터 동작 및 센서 대기 함수
 * @return 0: 성공(센서 도달 또는 모터 정지), -1: 실패/타임아웃/비상정지
 */
static int wait_motor_operation(int sel, int mt, int timeout, int status)
{
    vTaskDelay(pdMS_TO_TICKS(40));  
    
    int64_t start, end, elapsed;
    start = esp_timer_get_time();
    int ret = 0;
    do {
        if (emergency_stop_flag) return -1;
    
        vTaskDelay(pdMS_TO_TICKS(10));
        ret = pt_check(sel, mt);

        if (ret == 1 && status == MT_OPEN) return 0;
        else if (ret == -1 && status == MT_CLOSE) return 0;
        
        end = esp_timer_get_time();
        elapsed = (end - start) / 1000;
        if (elapsed >= timeout) return -1;   
        
        if (sel == DC_MOTOR) {
            if (!get_dcmotor_run(mt)) return 0;
        } else if (sel == STEP_MOTOR) {
            if (!get_stepmotor_run(mt)) return 0;
        }
    } while(1);

    return -1;
}

int motor_main_cover_test(int dir)
{
#ifdef FEATURE_MAIN_COVER   
    mt_message_t msg = {0};
    send_motor_msg(&msg, EMERGENCY_RESET_CMD, 0, 0, 0);
    if(dir == 1) {
        drive_main_cover_soft(FORWARD, 100);
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_OPEN);
        send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
    } else if(dir == -1) {
        drive_main_cover_soft(REVERSE, 100);
        vTaskDelay(pdMS_TO_TICKS(100));
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
        vTaskDelay(pdMS_TO_TICKS(150));
        wait_motor_operation(STEP_MOTOR, WASTE_COVER_MOTOR, 15000, MT_OPEN);
    } else if(dir == -1) {
        send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, REVERSE, 15000);
        vTaskDelay(pdMS_TO_TICKS(150));
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
    
    cnt = ms / 100;
    if((ms % 100) != 0) cnt++;
    
    do {
        if (emergency_stop_flag) return -1;
        vTaskDelay(pdMS_TO_TICKS(100));
    } while(--cnt > 0);
    
    return 0;
}

static void clear_all_motor_queues(void)
{
    if (motor_msg != NULL) {
        xQueueReset(motor_msg);
    }
}

int do_clean(void *arg)
{
    mt_message_t msg = {0};
    message_t led_msg = {0};
    msg.task_id = (uint32_t)arg;
    led_msg.task_id = (uint32_t)arg;

    uint32_t first_spin_speed = 140;
    uint32_t normal_speed = 100;

    clear_emergency_stop();
    clear_all_motor_queues();
    spin_mt_always_on = false;

    send_led_cmd_msg(&led_msg, LED_CLEANING_CMD);
    set_tof_sensor_enable(true);
    send_motor_msg(&msg, EMERGENCY_RESET_CMD, 0, 0, 0);
    
    send_plate_msg(&msg, PLATE_CMD, 0, FORWARD, 0);
    
#ifdef FEATURE_MAIN_COVER   
    // 01단계: MAIN_COVER 열기 (강파워 300ms Kick-Start 및 소프트 가속 적용)
    drive_main_cover_soft(FORWARD, 100);
    vTaskDelay(pdMS_TO_TICKS(150));

    if (wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 20000, MT_OPEN) != 0)
    {
        printf("\r\n\033[1;33m[ERROR] MAIN_COVER Open Timeout (20s)!\033[0m\r\n");
        goto estop_exit;
    }

    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
    vTaskDelay(pdMS_TO_TICKS(50));

    if (pt_check(DC_MOTOR, MAIN_COVER_MOTOR) != 1)
    {
        printf("\r\n\033[1;33m[ERROR] MAIN_COVER Sensor State Error!\033[0m\r\n");
        goto estop_exit;
    }
#endif

    spin_mt_always_on = true;

    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 220, FORWARD, first_spin_speed, 5000, false);
    if (wait_duration(5000) != 0) goto estop_exit;
    CHECK_ESTOP(); 
    
    // 02단계: 스쿱 OUT
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 15000);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 15000, MT_OPEN) != 0) goto estop_exit;
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
    CHECK_ESTOP(); 

    if (wait_duration(90 * 1000) != 0) goto estop_exit;
    CHECK_ESTOP();
    
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 80, REVERSE, normal_speed, 10000, false);
    vTaskDelay(pdMS_TO_TICKS(150));
    send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, FORWARD, 15000);
    vTaskDelay(pdMS_TO_TICKS(150));

#ifdef FEATURE_SHAKE_SCOOP
    if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 5000, MT_MIDDLE) != 0) goto estop_exit;

    for (int i = 0; i < 4; i++)
    {
        send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 30, REVERSE, normal_speed, 1000, false);
        if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 2000, MT_MIDDLE) != 0) goto estop_exit;

        send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 30, FORWARD, normal_speed, 1000, false);
        if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 2000, MT_MIDDLE) != 0) goto estop_exit;

        CHECK_ESTOP();
    }
#endif
    
    if (wait_motor_operation(STEP_MOTOR, WASTE_COVER_MOTOR, 15000, MT_OPEN) != 0)
    {
        printf("\r\n\033[1;33m[ERROR] WASTE_COVER Open Timeout!\033[0m\r\n");
        goto estop_exit;
    }
    send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);

    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 10000);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000, MT_CLOSE) != 0) goto estop_exit;
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

    // 09단계 쓰레기 배출 후 복귀 회전: 160도 REVERSE
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 130 + 30, REVERSE, normal_speed, 10000, false);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 10000, MT_MIDDLE) != 0) goto estop_exit;

    // 10단계 정회전 보정: 40도 FORWARD
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 40, FORWARD, normal_speed, 1000, false);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 5000, MT_MIDDLE) != 0) goto estop_exit;

    // 11단계: 스쿱 OUT
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 15000);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 15000, MT_OPEN) != 0) goto estop_exit;
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

    // 12단계: 쓰레기통 커버 닫기
    send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, REVERSE, 15000);

    // 13-1단계: M_PLATE 역방향 구동
    send_plate_msg(&msg, PLATE_CMD, 0, REVERSE, 0);

    // 13-2단계: 스쿱 모래 고르기 회전 (180도 FORWARD) - 타임아웃 25초
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 180, FORWARD, normal_speed, 5000, false);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 25000, MT_MIDDLE) != 0) goto estop_exit;

    if (wait_duration(60 * 1000) != 0) goto estop_exit;
    CHECK_ESTOP();

    // 14단계: 모래 정리 역회전 (230도 REVERSE = 180 + 50) - 타임아웃 25초
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 180 + 50, REVERSE, normal_speed, 5000, false);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 25000, MT_MIDDLE) != 0) goto estop_exit;

    // 15단계: 위치 복귀 (80도 FORWARD)
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 80, FORWARD, normal_speed, 3000, false);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 8000, MT_MIDDLE) != 0) goto estop_exit;

    spin_mt_always_on = false;

    // 16단계: 스쿱 IN 회수 - 타임아웃 25초
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 25000);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 25000, MT_CLOSE) != 0) goto estop_exit;
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

    // 17단계: 최종 원점 복귀 회전 (50도 REVERSE로 수정 적용)
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 50, REVERSE, normal_speed, 5000, false);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 8000, MT_MIDDLE) != 0) goto estop_exit;

#ifdef FEATURE_MAIN_COVER
    // 18단계: MAIN_COVER 닫기 (강파워 소프트 가속 적용)
    drive_main_cover_soft(REVERSE, 100);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_CLOSE) != 0) goto estop_exit;
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
#endif

    send_plate_msg(&msg, PLATE_CMD, 0, STOP, 0);
    set_tof_sensor_enable(false);
    send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
    return 0;

estop_exit:    
    printf("\r\n\033[1;33m[EMERGENCY] Emergency Stop Triggered! Cleaning process aborted.\033[0m\r\n");

    spin_mt_always_on = false;    
    set_tof_sensor_enable(false);    

    clear_all_motor_queues();

    send_motor_msg(&msg, EMERGENCY_RESET_CMD, 0, 0, 0);
    send_plate_msg(&msg, PLATE_CMD, 0, STOP, 0);
#ifdef FEATURE_MAIN_COVER
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
#endif
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
    send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);

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
    drive_main_cover_soft(FORWARD, 100);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_OPEN) != 0) goto manage_start_exit;
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
#endif

    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 10000);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000, MT_OPEN) != 0) goto manage_start_exit;
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
    set_tof_sensor_enable(false);

    return 0;

manage_start_exit:
    set_tof_sensor_enable(false);
    return -1;
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
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000, MT_CLOSE) != 0) goto manage_finish_exit;
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

#ifdef FEATURE_MAIN_COVER   
    drive_main_cover_soft(REVERSE, 100);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_CLOSE) != 0) goto manage_finish_exit;
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
#endif
    set_tof_sensor_enable(false);
    send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
    return 0;

manage_finish_exit:
    set_tof_sensor_enable(false);
    return -1;
}

void send_motor_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout)
{
    if (motor_msg == NULL) return;
    
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

    // 부팅 완료 후 LED 대기(IDLE) 신호 전송
    message_t led_msg = {0};
    send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
}

void set_emergency_stop(void)
{
    emergency_stop_flag = true;
    mt_message_t msg = {0};
    
    clear_all_motor_queues();

    send_plate_msg(&msg, PLATE_CMD, 0, STOP, 0);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
#ifdef FEATURE_MAIN_COVER
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
#endif
    send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 0, STOP, 0);
}

void clear_emergency_stop(void)
{
    emergency_stop_flag = false;
}