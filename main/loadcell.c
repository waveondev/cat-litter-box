#include "main.h"

static const char *TAG = "LCELL";

#define MAIN_CH 2
#define WASTE_CH 2

// --- 필터 및 타이머 설정 (100ms 주기 기준) ---
#define STABLE_TICK_TARGET   30    // 3초 (100ms * 30) : 이 시간 동안 변화가 없어야 안정화
#define AUTO_TARE_TICK_LIMIT 1800  // 3분 (100ms * 1800) : 유휴 상태 지속 시 자동 영점
#define NOISE_THRESHOLD      1500  // ADC 흔들림 허용 범위
#define DEADBAND_GRAMS       30.0f // 30g 이하의 미세 신호는 0g으로 마스킹 (노이즈/마찰 제거)

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
static QueueHandle_t loadcell_msg = NULL;

extern int current_state; 

// [수정] 컴파일 에러 해결을 위한 함수 사전 선언
void loadcell_cmd_task(void *arg);

void init_loadcell_global(void)
{
    if (xSemaphoreTake(cell_mutex, portMAX_DELAY) == pdTRUE) 
    {
        current_lc_state = STATE_IDLE;
        stable_counter = 0;
        idle_counter = 0;
        xSemaphoreGive(cell_mutex);
    }
}

void loadcell_init(void)
{
    ESP_LOGI(TAG, "%s", __func__);  
    loadcell_msg = xQueueCreate(10, sizeof(message_t)); 
    cell_mutex = xSemaphoreCreateMutex();
    init_loadcell_global();
    
    // 이제 컴파일러가 loadcell_cmd_task를 인식합니다.
    xTaskCreate(loadcell_cmd_task, "loadcell_cmd_task", 4096, NULL, 5, NULL);    
}

void send_loadcell_msg(void *message, uint32_t cmd)
{
    message_t *msg = (message_t *)message;
    msg->cmd = cmd;
    xQueueSend(loadcell_msg, msg, pdMS_TO_TICKS(100));
}

void loadcell_proc(int *values)
{
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

    if (xSemaphoreTake(cell_mutex, portMAX_DELAY) == pdTRUE) {
        
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
    if (xSemaphoreTake(cell_mutex, portMAX_DELAY) == pdTRUE) {
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
    if (xSemaphoreTake(cell_mutex, portMAX_DELAY) == pdTRUE) {
        if (mode == LOADCELL_MAIN) {
            ret_weight = final_main_weight;
        } else if (mode == LOADCELL_WASTE) {
            ret_weight = final_waste_weight;
        }
        xSemaphoreGive(cell_mutex);
    }
    return ret_weight;
}
