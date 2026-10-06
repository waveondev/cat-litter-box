#include "main.h"
#include "loadcell.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG __attribute__((unused)) = "LCELL";

#define MAIN_CH 2
#define WASTE_CH 2

extern bool clean_pause_flag;
extern bool clean_resume_recovery_requested;
extern int current_state; 

#define STABLE_TICK_TARGET    30    
#define AUTO_TARE_TICK_LIMIT 1800  
#define NOISE_THRESHOLD      1500  
#define DEADBAND_GRAMS        30.0f 

// --- 고양이 감지 링 버퍼 및 FSM 설정 ---
#define CAT_RING_BUFFER_SIZE  200    
#define CAT_INIT_SAMPLE_COUNT 30     // 초기 Baseline 인정 수집 샘플 수 (약 9초)
#define CAT_ENTRY_THRESHOLD_G 1000.0f // 진입 임계값 (+1kg)
#define CAT_EXIT_THRESHOLD_G  300.0f  // 이탈 임계값 (+300g 이내)
#define CAT_CLEAN_DELAY_SEC   180    // 🌟 자동 청소 대기시간 (3분 = 180초)

// --- 1단계 / 3단계 구조 하중 보정 계수 파라미터 ---
static const int64_t STAGE1_OFFSET_V2 = 144607;
static const int64_t STAGE1_OFFSET_V3 = 330013;
static const int64_t STAGE1_OFFSET_V4 = 135682;
static const int64_t STAGE1_OFFSET_V5 = 212343;

static const double WEIGHT_GAIN_V2 = 0.714; 
static const double WEIGHT_GAIN_V3 = 1.244; 
static const double WEIGHT_GAIN_V4 = 1.208; 
static const double WEIGHT_GAIN_V5 = 1.029; 

extern void save_lc_calibration_to_nvs(int state, int *offsets);

typedef enum {
    STATE_IDLE,    
    STATE_MOVING,  
    STATE_STABLE   
} LoadcellState;

static LoadcellState current_lc_state = STATE_IDLE;
static int stable_counter = 0;
static int idle_counter = 0;

static int64_t current_main_sum = 0;
static int64_t current_waste_sum = 0;
static int64_t base_main_sum = 0;   
static int64_t base_waste_sum = 0;  

static double final_main_weight = 0.0f;
static double final_waste_weight = 0.0f;

static const double SCALE_MAIN = 0.010416; 
static const double SCALE_WASTE = 0.006666; 

static SemaphoreHandle_t cell_mutex = NULL;
QueueHandle_t loadcell_msg = NULL;

static cat_detect_state_t g_cat_state = CAT_STATE_INIT;
static float g_ring_buffer[CAT_RING_BUFFER_SIZE] = {0.0f};
static int g_ring_head = 0;
static int g_ring_count = 0;
static double g_ring_sum = 0.0;
static float g_baseline_weight = 0.0f;
static int64_t g_exit_timestamp_us = 0;
static bool g_prev_clean_running = false;

// 🌟 [로그 제어 변수]: 부팅/초기화 시 1회 출력 플래그 및 타이머 기록 변수
static bool g_init_log_printed = false;
static int g_last_logged_min = 0;      

static float g_log_idle_baseline = 0.0f; 
static float g_log_peak_weight = 0.0f;   
static float g_log_exit_weight = 0.0f;   

static void reset_cat_ring_buffer(void)
{
    memset(g_ring_buffer, 0, sizeof(g_ring_buffer));
    g_ring_head = 0;
    g_ring_count = 0;
    g_ring_sum = 0.0;
    g_baseline_weight = 0.0f;
    g_cat_state = CAT_STATE_INIT;
    g_init_log_printed = false;
}

static float push_cat_ring_buffer(float new_val)
{
    if (g_ring_count < CAT_RING_BUFFER_SIZE) {
        g_ring_buffer[g_ring_head] = new_val;
        g_ring_sum += new_val;
        g_ring_count++;
        g_ring_head = (g_ring_head + 1) % CAT_RING_BUFFER_SIZE;
    } else {
        g_ring_sum -= g_ring_buffer[g_ring_head];
        g_ring_buffer[g_ring_head] = new_val;
        g_ring_sum += new_val;
        g_ring_head = (g_ring_head + 1) % CAT_RING_BUFFER_SIZE;
    }
    
    return (float)(g_ring_sum / g_ring_count);
}

static void process_cat_detection_fsm(float current_weight)
{
    bool clean_running = is_clean_running();

    if (g_prev_clean_running && !clean_running) {
        reset_cat_ring_buffer();
    }
    g_prev_clean_running = clean_running;

    if (clean_running || clean_pause_flag || clean_resume_recovery_requested) {
        return;
    }

    switch (g_cat_state)
    {
        case CAT_STATE_INIT:
            g_baseline_weight = push_cat_ring_buffer(current_weight);
            
            // 🌟 부팅/Reset 후 State: 0 수치를 최초 1회만 출력
            if (!g_init_log_printed) {
                g_init_log_printed = true;
                printf("[WEIGHT STATE CHANGED] Main: %.1fg | Base: %.1fg | State: %d (CAT_STATE_INIT)\n", 
                       current_weight, g_baseline_weight, g_cat_state);
                fflush(stdout);
            }

            if (g_ring_count >= CAT_INIT_SAMPLE_COUNT) {
                g_cat_state = CAT_STATE_IDLE;
                
                printf("=================================================\n");
                printf(" [CAT FSM STATE CHANGED] -> State: 1 (CAT_STATE_IDLE)\n");
                printf("  └─ Baseline Established : %.1fg\n", g_baseline_weight);
                printf("=================================================\n");
                fflush(stdout);
            }
            break;

        case CAT_STATE_IDLE:
        {
            float prev_base = g_baseline_weight;
            if (current_weight <= (prev_base - CAT_ENTRY_THRESHOLD_G)) {
                printf("[CAT FSM] Sudden -1kg Drop! Current: %.1fg, Base: %.1fg -> Resetting Buffer\n", current_weight, prev_base);
                fflush(stdout);
                reset_cat_ring_buffer();
                break;
            }

            g_baseline_weight = push_cat_ring_buffer(current_weight);

            if (current_weight >= (g_baseline_weight + CAT_ENTRY_THRESHOLD_G)) {
                g_cat_state = CAT_STATE_OCCUPIED;
                g_log_idle_baseline = g_baseline_weight;
                g_log_peak_weight = current_weight;

                float added = current_weight - g_baseline_weight;
                printf("=================================================\n");
                printf(" [CAT FSM STATE CHANGED] -> State: 2 (CAT_STATE_OCCUPIED)\n");
                printf("  └─ IDLE Baseline : %.1fg\n", g_baseline_weight);
                printf("  └─ Current Weight: %.1fg\n", current_weight);
                printf("  └─ Net Cat Weight: +%.1fg (CAT ENTERED!)\n", added);
                printf("=================================================\n");
                fflush(stdout);
            }
            break;
        }

        case CAT_STATE_OCCUPIED:
            if (current_weight > g_log_peak_weight) {
                g_log_peak_weight = current_weight;
            }

            if (current_weight <= (g_baseline_weight + CAT_EXIT_THRESHOLD_G)) {
                g_cat_state = CAT_STATE_WAIT_CLEAN;
                g_exit_timestamp_us = esp_timer_get_time();
                g_last_logged_min = 0;
                g_log_exit_weight = current_weight;

                float net_cat = g_log_peak_weight - g_log_idle_baseline;
                printf("=================================================\n");
                printf(" [CAT FSM STATE CHANGED] -> State: 3 (CAT_STATE_WAIT_CLEAN)\n");
                printf("  └─ Baseline      : %.1fg\n", g_log_idle_baseline);
                printf("  └─ Cat Peak      : %.1fg (Net Cat: %.1fg)\n", g_log_peak_weight, net_cat);
                printf("  └─ Exit Weight   : %.1fg\n", g_log_exit_weight);
                printf("  └─ Countdown     : 3-Min (180s) Auto-Clean Timer Started\n");
                printf("=================================================\n");
                fflush(stdout);
            }
            break;

        case CAT_STATE_WAIT_CLEAN:
            g_baseline_weight = push_cat_ring_buffer(current_weight);

            if (current_weight >= (g_baseline_weight + CAT_ENTRY_THRESHOLD_G)) {
                g_cat_state = CAT_STATE_OCCUPIED;
                printf("=================================================\n");
                printf(" [CAT FSM STATE CHANGED] -> State: 2 (CAT_STATE_OCCUPIED)\n");
                printf("  └─ Cat RE-ENTERED! Timer Canceled.\n");
                printf("=================================================\n");
                fflush(stdout);
                break;
            }

            int64_t elapsed_sec = (esp_timer_get_time() - g_exit_timestamp_us) / 1000000;
            int current_elapsed_min = (int)(elapsed_sec / 60);

            // 🌟 3분 대기 기준 1분 간격 남은 시간 출력
            if (current_elapsed_min > g_last_logged_min && current_elapsed_min <= 3) {
                g_last_logged_min = current_elapsed_min;
                int remaining_min = 3 - current_elapsed_min;
                if (remaining_min > 0) {
                    printf("[CAT FSM] Auto-Clean Timer: %d minute(s) remaining...\n", remaining_min);
                    fflush(stdout);
                }
            }

            // 🌟 3분(180초) 만료 체킹
            if (elapsed_sec >= CAT_CLEAN_DELAY_SEC) {
                float net_cat = g_log_peak_weight - g_log_idle_baseline;
                printf("=================================================\n");
                printf(" [AUTO CLEANING STARTED - CAT EVENT SUMMARY]\n");
                printf("  1) IDLE Baseline: %.1fg\n", g_log_idle_baseline);
                printf("  2) Cat Weight   : %.1fg (Peak: %.1fg)\n", net_cat, g_log_peak_weight);
                printf("  3) Exit Weight  : %.1fg\n", g_log_exit_weight);
                printf("  --> 3-min Expired! Triggering UI_CLEAN_CMD Now.\n");
                printf("=================================================\n");
                fflush(stdout);

                g_cat_state = CAT_STATE_IDLE;
                message_t msg = {0};
                send_ui_cmd_msg(&msg, UI_CLEAN_CMD);
            }
            break;
    }
}

cat_detect_state_t get_cat_detect_state(void) { return g_cat_state; }
float get_cat_baseline_weight(void) { return g_baseline_weight; }

void init_loadcell_global(void)
{
    if (cell_mutex != NULL && xSemaphoreTake(cell_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        current_lc_state = STATE_IDLE;
        stable_counter = 0;
        idle_counter = 0;
        xSemaphoreGive(cell_mutex);
    }
}

void loadcell_init(void)
{
    esp_log_level_set("LCELL", ESP_LOG_INFO);
    if (cell_mutex == NULL) cell_mutex = xSemaphoreCreateMutex();
    if (loadcell_msg == NULL) loadcell_msg = xQueueCreate(10, sizeof(message_t));
    reset_cat_ring_buffer();
    printf("[LCELL] loadcell_init OK\n");
    fflush(stdout);
}

void send_loadcell_msg(void *message, uint32_t cmd)
{
    if (loadcell_msg == NULL) return;
    message_t *msg = (message_t *)message;
    msg->cmd = cmd;
    xQueueSend(loadcell_msg, msg, pdMS_TO_TICKS(100));
}

void loadcell_proc(int *values)
{
    if (values == NULL) return;

    double raw2_corr = ((double)values[2] - STAGE1_OFFSET_V2) * WEIGHT_GAIN_V2;
    double raw3_corr = ((double)values[3] - STAGE1_OFFSET_V3) * WEIGHT_GAIN_V3;
    double raw4_corr = ((double)values[4] - STAGE1_OFFSET_V4) * WEIGHT_GAIN_V4;
    double raw5_corr = ((double)values[5] - STAGE1_OFFSET_V5) * WEIGHT_GAIN_V5;

    int64_t new_main_sum = (int64_t)(raw2_corr + raw3_corr + raw4_corr + raw5_corr);
    int64_t new_waste_sum = (int64_t)values[0] + values[1];

    current_main_sum = new_main_sum;
    current_waste_sum = new_waste_sum;

    double calc_main = (double)(current_main_sum - base_main_sum) * SCALE_MAIN;
    double temp_main_weight = (calc_main > -DEADBAND_GRAMS && calc_main < DEADBAND_GRAMS) ? 0.0f : calc_main;

    double calc_waste = (double)(current_waste_sum - base_waste_sum) * SCALE_WASTE;
    double temp_waste_weight = (calc_waste > -DEADBAND_GRAMS && calc_waste < DEADBAND_GRAMS) ? 0.0f : calc_waste;

    process_cat_detection_fsm((float)temp_main_weight);

    if (cell_mutex != NULL && xSemaphoreTake(cell_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        final_main_weight = temp_main_weight;
        final_waste_weight = temp_waste_weight;
        xSemaphoreGive(cell_mutex);
    }
}

static void execute_tare(int state)
{
    if (cell_mutex != NULL && xSemaphoreTake(cell_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        base_main_sum = current_main_sum;
        base_waste_sum = current_waste_sum;
        if (state > 0) {
            int save_offsets[4] = { (int)(base_main_sum / 4), (int)(base_main_sum / 4), (int)(base_main_sum / 4), (int)(base_main_sum / 4) };
            save_lc_calibration_to_nvs(state, save_offsets);
        }
        reset_cat_ring_buffer();
        xSemaphoreGive(cell_mutex);
    }
}

void loadcell_cmd_task(void *arg) 
{
    while (1) {
        message_t msg = {0};
        if (loadcell_msg && xQueueReceive(loadcell_msg, &msg, portMAX_DELAY) == pdPASS) {
            if ((int)msg.cmd >= LOADCELL_TARE_CMD && msg.cmd <= LOADCELL_SAVE_STATE3_CMD) {
                execute_tare(msg.cmd - 1);
            }
        }
    }
    vTaskDelete(NULL);
}

double get_weight(int mode)
{
    double ret_weight = 0.0f;
    if (cell_mutex != NULL && xSemaphoreTake(cell_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (mode == LOADCELL_MAIN) ret_weight = final_main_weight;
        else if (mode == LOADCELL_WASTE) ret_weight = final_waste_weight;
        xSemaphoreGive(cell_mutex);
    }
    return ret_weight;
}