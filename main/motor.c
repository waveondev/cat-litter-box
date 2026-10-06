#include "main.h"
#include "sensor.h"

static const char *TAG = "MOTOR";

// 전역 변수 및 플래그 정의
static bool spin_mt_always_on = false;

volatile bool emergency_stop_flag = false;
volatile bool clean_running_flag = false;
volatile bool clean_pause_flag = false;
volatile bool clean_resume_recovery_requested = false;

// 전방 선언
static int wait_motor_operation(int sel, int mt, int timeout, int status);
static void drive_main_cover_soft(int dir, uint32_t target_speed);
int motor_calibration_recovery(void);

#define CHECK_ESTOP() \
    do { \
        if (emergency_stop_flag || clean_resume_recovery_requested) { \
            ESP_LOGW(TAG, "Emergency Stop or Recovery Exit Requested! Aborting scenario."); \
            goto estop_exit; \
        } \
    } while(0)

#define CHECK_TOF_PAUSE(target_label) \
    do { \
        int tof_dist = get_fresh_tof_distance(); \
        if (tof_dist > 0 && tof_dist <= 550) { \
            ESP_LOGW(TAG, "================================================="); \
            ESP_LOGW(TAG, "[TOF SAFETY] Obstacle/Cat Detected! Distance: %d mm (Threshold <= 550 mm)", tof_dist); \
            ESP_LOGW(TAG, "[TOF SAFETY] FORCING IMMEDIATE PAUSE & STOPPING ALL MOTORS!"); \
            ESP_LOGW(TAG, "================================================="); \
            \
            mt_message_t pause_msg = {0}; \
            message_t pause_led_msg = {0}; \
            send_plate_msg(&pause_msg, PLATE_CMD, 0, STOP, 0); \
            send_scpspin_motor_msg_ex(&pause_msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false); \
            send_scpinout_msg(&pause_msg, SCP_INOUT_CMD, 0, STOP, 0); \
            send_waste_motor_msg(&pause_msg, WASTE_COVER_CMD, 0, STOP, 0); \
            send_main_motor_msg(&pause_msg, MAIN_COVER_CMD, 0, STOP, 0); \
            send_led_cmd_msg(&pause_led_msg, LED_PAUSE_CMD); \
            \
            clean_pause_flag = true; \
            \
            while (clean_pause_flag) { \
                if (emergency_stop_flag || clean_resume_recovery_requested) { \
                    goto target_label; \
                } \
                vTaskDelay(pdMS_TO_TICKS(100)); \
            } \
        } else { \
            ESP_LOGI(TAG, "[TOF SAFETY] Clear! Distance (%d mm) > 550 mm. Safe to proceed.", tof_dist); \
        } \
    } while(0)

int pt_check(int sel, int mt)
{
    int status = get_pt_status();

    if (sel == STEP_MOTOR) {
        if (mt == WASTE_COVER_MOTOR) {
            if (status & PT_BIT_WASTE_CLOSE) return -1;
            else if (status & PT_BIT_WASTE_OPEN) return 1;
        } else if (mt == SCPSPIN_MOTOR) {
            if ((status & PT_BIT_SCP_SPIN_ST) != 0 && (status & PT_BIT_SCP_SPIN_ED) != 0) return 1;
        }
    } else if (sel == DC_MOTOR) {
        if (mt == MAIN_COVER_MOTOR) {
            if (status & PT_BIT_MCOVER_CLOSE) return -1;
            else if (status & PT_BIT_MCOVER_OPEN) return 1;
        }
        if (mt == SCPINOUT_MOTOR) {
            if (status & PT_BIT_SCP_IN) return -1;
            else if (status & PT_BIT_SCP_OUT) return 1;
        }
    }
    return 0;
}

bool check_spin_mt_on(void)
{
    return spin_mt_always_on;
}

bool is_clean_running(void)
{
    return clean_running_flag;
}

void toggle_clean_pause(void)
{
    message_t led_msg = {0};
    mt_message_t msg = {0};

    if (!clean_pause_flag) {
        clean_pause_flag = true;
        ESP_LOGW(TAG, "[PAUSE] Cleaning Pause Triggered! Stopping ALL motors...");
        
        send_plate_msg(&msg, PLATE_CMD, 0, STOP, 0);
        send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);
        send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
        send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
        send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);

        send_led_cmd_msg(&led_msg, LED_PAUSE_CMD);
    } else {
        ESP_LOGI(TAG, "[RESUME] Cleaning Button Pressed during PAUSE. Requesting do_clean Exit & Recovery...");
        clean_pause_flag = false;
        clean_resume_recovery_requested = true;
    }
}

void set_emergency_stop(void)
{
    emergency_stop_flag = true;
    mt_message_t msg = {0};

    send_plate_msg(&msg, PLATE_CMD, 0, STOP, 0);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
    send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);
}

void clear_emergency_stop(void)
{
    emergency_stop_flag = false;
}

static void drive_main_cover_soft(int dir, uint32_t target_speed)
{
    mt_message_t msg = {0};
    vTaskDelay(pdMS_TO_TICKS(50));

    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, dir, 100);
    vTaskDelay(pdMS_TO_TICKS(300));

    uint32_t start_speed = 70;
    if (target_speed < start_speed) start_speed = target_speed;

    for (uint32_t duty = start_speed; duty <= target_speed; duty += 10) {
        send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, dir, duty);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

static int wait_motor_operation(int sel, int mt, int timeout, int status)
{
    vTaskDelay(pdMS_TO_TICKS(150));  
    
    int64_t start = esp_timer_get_time();

    do {
        if (emergency_stop_flag || clean_resume_recovery_requested) return -1;

        while (clean_pause_flag) {
            if (emergency_stop_flag || clean_resume_recovery_requested) return -1;
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (emergency_stop_flag || clean_resume_recovery_requested) return -1;

        vTaskDelay(pdMS_TO_TICKS(20));
        int ret = pt_check(sel, mt);

        bool is_reached = false;

        if (ret == 1 && status == MT_OPEN) is_reached = true;
        else if (ret == -1 && status == MT_CLOSE) is_reached = true;

        int64_t elapsed_ms = (esp_timer_get_time() - start) / 1000;
        if (elapsed_ms > 300) {
            if (sel == DC_MOTOR) {
                if (!get_dcmotor_run(mt)) is_reached = true;
            } else if (sel == STEP_MOTOR) {
                if (!get_stepmotor_run(mt)) is_reached = true;
            }
        }

        if (is_reached) {
            return 0; 
        }

        if (elapsed_ms >= timeout) return -1;   

    } while(1);

    return -1;
}

static int wait_duration(int ms)
{
    int cnt;
    if (ms < 100) return -1;
    
    cnt = ms / 100;
    if ((ms % 100) != 0) cnt++;
    
    do {
        if (emergency_stop_flag || clean_resume_recovery_requested) return -1;

        while (clean_pause_flag) {
            if (emergency_stop_flag || clean_resume_recovery_requested) return -1;
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    } while (--cnt > 0);
    
    return 0;
}

static int get_fresh_tof_distance(void)
{
    set_tof_sensor_enable(true);
    reset_tof_ring_buffer();

    int wait_ms = 0;
    while (get_tof_ring_count() < 5 && wait_ms < 2000) {
        vTaskDelay(pdMS_TO_TICKS(10));
        wait_ms += 10;
    }

    int valid_dist = get_tof_distance();

    if (valid_dist >= 0 && valid_dist <= 2000) {
        ESP_LOGI(TAG, "[TOF RING BUFFER] Valid Distance: %d mm (Samples: %d)", valid_dist, get_tof_ring_count());
    } else {
        ESP_LOGW(TAG, "[TOF RING BUFFER] Invalid/Error Distance (%d mm)", valid_dist);
        valid_dist = -1;
    }

    return valid_dist;
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
    spin_mt_always_on = false;

    clean_running_flag = true;
    clean_pause_flag = false;
    clean_resume_recovery_requested = false;

    send_led_cmd_msg(&led_msg, LED_CLEANING_CMD);
    set_tof_sensor_enable(true);
    
    send_plate_msg(&msg, PLATE_CMD, 0, FORWARD, 0);
    
    ESP_LOGI(TAG, "[CLEAN Step 01] Opening MAIN_COVER...");
    drive_main_cover_soft(FORWARD, 100);
    vTaskDelay(pdMS_TO_TICKS(300));

    int wait_cov_ret = wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 20000, MT_OPEN);
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    if (wait_cov_ret != 0)
    {
        ESP_LOGE(TAG, "[ERROR] MAIN_COVER Open Timeout (20s)!");
        goto estop_exit;
    }

    if (pt_check(DC_MOTOR, MAIN_COVER_MOTOR) != 1)
    {
        ESP_LOGE(TAG, "[ERROR] MAIN_COVER Open Limit Sensor NOT Detected!");
        goto estop_exit;
    }

    ESP_LOGI(TAG, "[CLEAN Step 01] MAIN_COVER Opened Successfully.");

    spin_mt_always_on = true;

    for (int cycle = 1; cycle <= 2; cycle++)
    {
        ESP_LOGI(TAG, "=================================================");
        ESP_LOGI(TAG, "[CLEAN] Starting Clean Cycle %d/2...", cycle);
        ESP_LOGI(TAG, "=================================================");

        if (cycle == 1) {
            send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 220, FORWARD, first_spin_speed, 5000, false);
        }
        
        if (wait_duration(5000) != 0) goto estop_exit;
        CHECK_ESTOP(); 
        
        send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 15000);
        vTaskDelay(pdMS_TO_TICKS(150));
        if (wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 15000, MT_OPEN) != 0) goto estop_exit;
        send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
        CHECK_ESTOP(); 

        if (wait_duration(90 * 1000) != 0) goto estop_exit;
        CHECK_ESTOP();
        
        send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 80, REVERSE, normal_speed, 10000, false);
        vTaskDelay(pdMS_TO_TICKS(150));
        if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 10000, MT_MIDDLE) != 0) goto estop_exit;
        
        send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, FORWARD, 15000);
        vTaskDelay(pdMS_TO_TICKS(300));

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
            ESP_LOGE(TAG, "[ERROR] WASTE_COVER Open Timeout!");
            goto estop_exit;
        }
        send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
        vTaskDelay(pdMS_TO_TICKS(200));

        CHECK_TOF_PAUSE(estop_exit);

        send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 10000);
        vTaskDelay(pdMS_TO_TICKS(150));
        if (wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000, MT_CLOSE) != 0) goto estop_exit;
        send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

        send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 160, REVERSE, normal_speed, 10000, false);
        vTaskDelay(pdMS_TO_TICKS(150));
        if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 10000, MT_MIDDLE) != 0) goto estop_exit;

        send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 70, FORWARD, normal_speed, 5000, false);
        vTaskDelay(pdMS_TO_TICKS(150));
        if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 5000, MT_MIDDLE) != 0) goto estop_exit;

        send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 15000);
        vTaskDelay(pdMS_TO_TICKS(150));
        if (wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 15000, MT_OPEN) != 0) goto estop_exit;
        send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

        if (pt_check(DC_MOTOR, SCPINOUT_MOTOR) != 1)
        {
            ESP_LOGW(TAG, "[SAFETY] Scoop OUT position not detected! Retrying Scoop OUT...");
            send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, FORWARD, 15000);
            vTaskDelay(pdMS_TO_TICKS(150));
            if (wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 15000, MT_OPEN) != 0) goto estop_exit;
            send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
        }

        send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, REVERSE, 15000);
        vTaskDelay(pdMS_TO_TICKS(150));

        if (cycle == 1)
        {
            ESP_LOGI(TAG, "[CLEAN] Triggering Step 14 (SCP_SPIN 170 deg) IMMEDIATELY with Waste Cover Close...");
            send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 170, FORWARD, first_spin_speed, 5000, false);
        }

        bool waste_close_success = false;
        for (int retry = 0; retry < 3; retry++)
        {
            if (retry > 0) {
                ESP_LOGW(TAG, "[SAFETY] WASTE_COVER Close failed! Retrying... (%d/3)", retry + 1);
                send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, FORWARD, 3000);
                vTaskDelay(pdMS_TO_TICKS(500));
                send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, REVERSE, 15000);
                vTaskDelay(pdMS_TO_TICKS(150));
            }

            if (wait_motor_operation(STEP_MOTOR, WASTE_COVER_MOTOR, 15000, MT_CLOSE) == 0)
            {
                send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
                vTaskDelay(pdMS_TO_TICKS(150));

                if (pt_check(STEP_MOTOR, WASTE_COVER_MOTOR) == -1)
                {
                    waste_close_success = true;
                    break;
                }
            }
        }

        if (!waste_close_success)
        {
            ESP_LOGE(TAG, "[ERROR] WASTE_COVER Close Sensor Error after Retries!");
            goto estop_exit;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 50, FORWARD, normal_speed, 8000, false);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 8000, MT_MIDDLE) != 0) goto estop_exit;

    spin_mt_always_on = false;

    CHECK_TOF_PAUSE(estop_exit);

    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 25000);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 25000, MT_CLOSE) != 0) goto estop_exit;
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, REVERSE, normal_speed, 8000, false);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 8000, MT_MIDDLE) != 0) goto estop_exit;

    ESP_LOGI(TAG, "[CLEAN Step 20-1] Performing Motor Calibration...");
    if (motor_calibration() != 0)
    {
        ESP_LOGE(TAG, "[ERROR] Motor Calibration Failed after Step 20!");
        goto estop_exit;
    }
    ESP_LOGI(TAG, "[CLEAN Step 20-1] Motor Calibration Completed Successfully.");

    drive_main_cover_soft(REVERSE, 100);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_CLOSE) != 0) goto estop_exit;
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);

    send_plate_msg(&msg, PLATE_CMD, 0, STOP, 0);
    set_tof_sensor_enable(false);
    
    clean_running_flag = false;
    clean_pause_flag = false;
    clean_resume_recovery_requested = false;
    send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
    return 0;

estop_exit:    
    ESP_LOGW(TAG, "[EMERGENCY] Emergency Stop Triggered or PAUSE Exit requested!");

    spin_mt_always_on = false;    
    bool is_recovery_mode = clean_resume_recovery_requested;
    clean_running_flag = false;
    clean_pause_flag = false;
    clean_resume_recovery_requested = false;

    send_plate_msg(&msg, PLATE_CMD, 0, STOP, 0);
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
    send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);

    if (is_recovery_mode)
    {
        return do_clean_resume_recovery();
    }

    send_led_cmd_msg(&led_msg, LED_ERROR_CMD); 
    return -1;
}

// 기존 로그 포맷대로 정돈된 오리진 복구 시나리오
int do_clean_resume_recovery(void)
{
    mt_message_t msg = {0};
    message_t led_msg = {0};
    
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "[RECOVERY] Starting Origin (Resume Recovery Scenario)...");
    ESP_LOGI(TAG, "=================================================");

    clean_pause_flag = false;
    vTaskDelay(pdMS_TO_TICKS(300));

    int pt = get_pt_status();
    bool is_waste_closed = ((pt & PT_BIT_WASTE_CLOSE) != 0);

    if (!is_waste_closed)
    {
        ESP_LOGI(TAG, "[RECOVERY] Step 1: Closing WASTE_COVER FIRST & Waiting Complete...");
        send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, REVERSE, 15000);
        vTaskDelay(pdMS_TO_TICKS(200));
        
        if (wait_motor_operation(STEP_MOTOR, WASTE_COVER_MOTOR, 15000, MT_CLOSE) != 0) goto recovery_abort;
        send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    ESP_LOGI(TAG, "[RECOVERY] Step 2: Performing SCOOP_SPIN Calibration...");
    if (motor_calibration_recovery() != 0) goto recovery_abort;
    vTaskDelay(pdMS_TO_TICKS(300));

    pt = get_pt_status();
    bool is_scp_in = ((pt & PT_BIT_SCP_IN) != 0);

    if (!is_scp_in)
    {
        ESP_LOGI(TAG, "[RECOVERY] Step 3: Executing SCP_INOUT IN...");
        send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 15000);
        vTaskDelay(pdMS_TO_TICKS(200));
        if (wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 15000, MT_CLOSE) != 0) goto recovery_abort;
        send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    pt = get_pt_status();
    bool is_mcover_open = ((pt & PT_BIT_MCOVER_OPEN) != 0);

    if (is_mcover_open)
    {
        ESP_LOGI(TAG, "[RECOVERY] Step 4: MAIN_COVER is OPEN. Executing MAIN_COVER CLOSE...");
        drive_main_cover_soft(REVERSE, 100);
        vTaskDelay(pdMS_TO_TICKS(200));
        if (wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_CLOSE) != 0) goto recovery_abort;
        send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    ESP_LOGI(TAG, "[RECOVERY] Step 5: Resetting System to Complete IDLE State...");
    
    clean_running_flag = false;
    clean_pause_flag = false;
    clean_resume_recovery_requested = false;
    emergency_stop_flag = false;

    set_tof_sensor_enable(false);

    send_plate_msg(&msg, PLATE_CMD, 0, STOP, 0);
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
    send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);

    send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
    return 0;

recovery_abort:
    clean_running_flag = false;
    clean_pause_flag = false;
    clean_resume_recovery_requested = false;

    send_plate_msg(&msg, PLATE_CMD, 0, STOP, 0);
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
    send_waste_motor_msg(&msg, WASTE_COVER_CMD, 0, STOP, 0);
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);
    set_tof_sensor_enable(false);
    send_led_cmd_msg(&led_msg, LED_ERROR_CMD);
    return -1;
}

static int scp_spin_find_st_origin(int search_dir)
{
    mt_message_t msg = {0};
    uint32_t fwd_dir = FORWARD;
    uint32_t rev_dir = REVERSE;
    uint32_t search_target_dir = (search_dir == 1) ? FORWARD : REVERSE;

    if (emergency_stop_flag) return -1;

    int current_pt = get_pt_status();
    bool on_st_sensor = ((current_pt & PT_BIT_SCP_SPIN_ST) != 0);

    if (on_st_sensor) {
        send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 360, rev_dir, 100, 30000, false);
        
        int64_t st_time = esp_timer_get_time();
        while (((esp_timer_get_time() - st_time) / 1000) < 30000) {
            if (emergency_stop_flag) {
                send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);
                return -1;
            }
            if ((get_pt_status() & PT_BIT_SCP_SPIN_ST) == 0) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);
        vTaskDelay(pdMS_TO_TICKS(100));

        if (emergency_stop_flag) return -1;

        send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 360, fwd_dir, 100, 30000, false);
        st_time = esp_timer_get_time();
        while (((esp_timer_get_time() - st_time) / 1000) < 30000) {
            if (emergency_stop_flag) {
                send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);
                return -1;
            }
            if ((get_pt_status() & PT_BIT_SCP_SPIN_ST) != 0) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);
    } 
    else {
        send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 360, search_target_dir, 100, 30000, false);

        int64_t st_time = esp_timer_get_time();
        while (((esp_timer_get_time() - st_time) / 1000) < 30000) {
            if (emergency_stop_flag) {
                send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);
                return -1;
            }
            if ((get_pt_status() & PT_BIT_SCP_SPIN_ST) != 0) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);
        vTaskDelay(pdMS_TO_TICKS(100));

        if (emergency_stop_flag) return -1;

        return scp_spin_find_st_origin(search_dir);
    }

    return 0;
}

int motor_calibration(void)
{
    mt_message_t msg = {0};
    clear_emergency_stop();

    int pt = get_pt_status();

    bool is_scp_in = ((pt & PT_BIT_SCP_IN) != 0);

    if (is_scp_in) {
        if (scp_spin_find_st_origin(1) != 0) goto calib_abort;
    } 
    else {
        if (scp_spin_find_st_origin(-1) != 0) goto calib_abort;
        if (emergency_stop_flag) goto calib_abort;

        send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 90, FORWARD, 100, 10000, false);
        vTaskDelay(pdMS_TO_TICKS(150));
        if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 10000, MT_MIDDLE) != 0) goto calib_abort;
        if (emergency_stop_flag) goto calib_abort;

        send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 15000);
        vTaskDelay(pdMS_TO_TICKS(150));
        if (wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 15000, MT_CLOSE) != 0) goto calib_abort;
        send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
        if (emergency_stop_flag) goto calib_abort;

        if (scp_spin_find_st_origin(-1) != 0) goto calib_abort;
    }

    if (emergency_stop_flag) goto calib_abort;

    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 70, REVERSE, 100, 10000, false);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 10000, MT_MIDDLE) != 0) goto calib_abort;
    if (emergency_stop_flag) goto calib_abort;

    return 0;

calib_abort:
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
    return -1;
}

int motor_calibration_recovery(void)
{
    mt_message_t msg = {0};
    clear_emergency_stop();

    if (scp_spin_find_st_origin(-1) != 0) goto calib_rec_abort;
    if (emergency_stop_flag) goto calib_rec_abort;
    vTaskDelay(pdMS_TO_TICKS(200));

    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 75, FORWARD, 100, 10000, false);
    vTaskDelay(pdMS_TO_TICKS(200));
    if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 10000, MT_MIDDLE) != 0) goto calib_rec_abort;
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);
    vTaskDelay(pdMS_TO_TICKS(300));
    if (emergency_stop_flag) goto calib_rec_abort;

    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 15000);
    vTaskDelay(pdMS_TO_TICKS(200));
    if (wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 15000, MT_CLOSE) != 0) goto calib_rec_abort;
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
    vTaskDelay(pdMS_TO_TICKS(300));
    if (emergency_stop_flag) goto calib_rec_abort;

    if (scp_spin_find_st_origin(-1) != 0) goto calib_rec_abort;
    vTaskDelay(pdMS_TO_TICKS(200));

    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 70, REVERSE, 100, 10000, false);
    vTaskDelay(pdMS_TO_TICKS(200));
    if (wait_motor_operation(STEP_MOTOR, SCPSPIN_MOTOR, 10000, MT_MIDDLE) != 0) goto calib_rec_abort;
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);
    vTaskDelay(pdMS_TO_TICKS(300));

    return 0;

calib_rec_abort:
    send_scpspin_motor_msg_ex(&msg, SCP_SPIN_CMD, 0, STOP, 0, 0, false);
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);
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

    drive_main_cover_soft(FORWARD, 100);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_OPEN) != 0) goto manage_start_exit;
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);

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
    
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, REVERSE, 10000);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(DC_MOTOR, SCPINOUT_MOTOR, 10000, MT_CLOSE) != 0) goto manage_finish_exit;
    send_scpinout_msg(&msg, SCP_INOUT_CMD, 0, STOP, 0);

    drive_main_cover_soft(REVERSE, 100);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (wait_motor_operation(DC_MOTOR, MAIN_COVER_MOTOR, 10000, MT_CLOSE) != 0) goto manage_finish_exit;
    send_main_motor_msg(&msg, MAIN_COVER_CMD, 0, STOP, 0);

    set_tof_sensor_enable(false);
    send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
    return 0;

manage_finish_exit:
    set_tof_sensor_enable(false);
    return -1;
}

int motor_scpspin_test(int dir)
{
    mt_message_t msg = {0};
    send_scpspin_motor_msg(&msg, SCP_SPIN_CMD, 360, (dir == 1) ? FORWARD : REVERSE, 15000);
    return 0;
}

void motor_init(void)
{
    message_t led_msg = {0};
    send_led_cmd_msg(&led_msg, LED_IDLE_CMD);
}