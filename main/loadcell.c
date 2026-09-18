#include "main.h"

static const char *TAG = "LCELL";

#define MAIN_CH 2
#define WASTE_CH 2

// --- 필터 및 타이머 설정 (100ms 주기 기준) ---
#define STABLE_TICK_TARGET    30    // 3초 (100ms * 30) : 수치 안정을 위한 카운트
#define AUTO_TARE_TICK_LIMIT 1800  // 3분 (100ms * 1800) : 영점 크리프 보정
#define NOISE_THRESHOLD      1500  // ADC 흔들림 감지 임계값
#define DEADBAND_GRAMS        30.0f // 30g 미만 불필요 수치 0g 클램핑

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

// ⭐ [단일화 및 널 방어] 뮤텍스 및 큐 핸들 정리
static SemaphoreHandle_t cell_mutex = NULL;
QueueHandle_t loadcell_msg = NULL;

extern int current_state; 

void loadcell_cmd_task(void *arg);

void init_loadcell_global(void)
{
    // ⭐ cell_mutex 널 방어 코드 추가
    if (cell_mutex != NULL && xSemaphoreTake(cell_mutex, pdMS_TO_TICKS(100)) == pdTRUE) 
    {
        current_lc_state = STATE_IDLE;
        stable_counter = 0;
        idle_counter = 0;
        xSemaphoreGive(cell_mutex);
    }
}

void loadcell_init(void)
{
    // ⭐ cell_mutex 생성 및 검증
    if (cell_mutex == NULL) {
        cell_mutex = xSemaphoreCreateMutex();
        if (cell_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create cell_mutex!");
            return;
        }
    }

    // ⭐ loadcell_msg 큐 생성 및 검증
    if (loadcell_msg == NULL) {
        loadcell_msg = xQueueCreate(10, sizeof(message_t));
        if (loadcell_msg == NULL) {
            ESP_LOGE(TAG, "Failed to create loadcell_msg Queue!");
            return;
        }
    }

    ESP_LOGI(TAG, "Loadcell Mutex & Queue initialized successfully.");
}

void send_loadcell_msg(void *message, uint32_t cmd)
{
    // ⭐ 큐 널 체크 방어
    if (loadcell_msg == NULL) {
        ESP_LOGW(TAG, "loadcell_msg queue is NULL, msg ignored.");
        return;
    }

    message_t *msg = (message_t *)message;
    msg->cmd = cmd;
    xQueueSend(loadcell_msg, msg, pdMS_TO_TICKS(100));
}

void loadcell_proc(int *values)
{
    if (values == NULL) return;

    int64_t new_main_sum = (int64_t)values[2] + values[3] + values[4] + values[5];
    int64_t new_waste_sum = (int64_t)values[0] + values[1];

    int64_t delta_main = llabs(new_main_sum - current_main_sum);

    current_main_sum = new_main_sum;
    current_waste_sum = new_waste_sum;

    if (delta_main > NOISE_THRESHOLD) {
        current_lc_state = STATE_MOVING;
        stable_counter = 0;
        idle_counter = 0;
    } else {
        if (current_lc_state == STATE_MOVING) {
            stable_counter++;
            if (stable_counter >= STABLE_TICK_TARGET) {
                current_lc_state = STATE_STABLE;
            }
        } else if (current_lc_state == STATE_STABLE || current_lc_state == STATE_IDLE) {
            current_lc_state = STATE_IDLE;
            idle_counter++;
            
            if (idle_counter >= AUTO_TARE_TICK_LIMIT && final_main_weight < 500.0) { 
                base_main_sum = current_main_sum; 
                idle_counter = 0; 
                ESP_LOGD(TAG, "Auto-Tare Applied (Creep compensation)");
            }
        }
    }

    // ⭐ cell_mutex 널 체크 방어
    if (cell_mutex != NULL && xSemaphoreTake(cell_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        
        double calc_main = (double)(current_main_sum - base_main_sum) * SCALE_MAIN;
        if (calc_main > -DEADBAND_GRAMS && calc_main < DEADBAND_GRAMS) {
            final_main_weight = 0.0f;
        } else {
            final_main_weight = calc_main;
        }

        double calc_waste = (double)(current_waste_sum - base_waste_sum) * SCALE_WASTE;
        if (calc_waste > -DEADBAND_GRAMS && calc_waste < DEADBAND_GRAMS) {
            final_waste_weight = 0.0f;
        } else {
            final_waste_weight = calc_waste;
        }

        xSemaphoreGive(cell_mutex);
    }
}

static void execute_tare(int state)
{
    // ⭐ cell_mutex 널 체크 방어
    if (cell_mutex != NULL && xSemaphoreTake(cell_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        base_main_sum = current_main_sum;
        base_waste_sum = current_waste_sum;
        
        ESP_LOGI(TAG, "Manual Tare Executed. Base updated.");

        if (state > 0) {
            int save_offsets[4] = {
                (int)(base_main_sum / 4), (int)(base_main_sum / 4), 
                (int)(base_main_sum / 4), (int)(base_main_sum / 4)
            };
            save_lc_calibration_to_nvs(state, save_offsets);
        }
        xSemaphoreGive(cell_mutex);
    }
}

void loadcell_cmd_task(void *arg) 
{
    ESP_LOGI(TAG, "%s +", __func__);
    while (1) {
        message_t msg = {0};
        
        // ⭐ 큐가 초기화될 때까지 대기
        if (loadcell_msg == NULL) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (xQueueReceive(loadcell_msg, &msg, portMAX_DELAY) == pdPASS) {
            switch((int)(msg.cmd))
            {
                case LOADCELL_TARE_CMD:
                    execute_tare(0);
                    break;
                case LOADCELL_SAVE_STATE1_CMD:
                    execute_tare(1);
                    break;
                case LOADCELL_SAVE_STATE2_CMD:
                    execute_tare(2);
                    break;
                case LOADCELL_SAVE_STATE3_CMD:
                    execute_tare(3);
                    break;
                default:
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}

double get_weight(int mode)
{
    double ret_weight = 0.0f;

    // ⭐ cell_mutex 널 체크 방어
    if (cell_mutex != NULL && xSemaphoreTake(cell_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (mode == LOADCELL_MAIN) {
            ret_weight = final_main_weight;
        } else if (mode == LOADCELL_WASTE) {
            ret_weight = final_waste_weight;
        }
        xSemaphoreGive(cell_mutex);
    }
    return ret_weight;
}