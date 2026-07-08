#include "main.h"

static const char *TAG = "DC_MOTOR_CTRL";

// 모터 방향 정의
//#define MT_DIR_NORMAL
#ifdef MT_DIR_NORMAL
#define MT_FORWARD_DIR  1
#define MT_REVERSE_DIR  0
#else
#define MT_FORWARD_DIR  0
#define MT_REVERSE_DIR  1
#endif

// 기본 하드웨어 핀 정의
#define SCP_INOUT_PWM           (8)
#define SCP_INOUT_SEL           (9)
#define PLATE_PWM               (45)
#define PLATE_SEL               (38)

#define SCP_INOUT_ADC_CH        (ADC_CHANNEL_2) // Unit 1 Ch 2
#define PLATE_ADC_CH            (ADC_CHANNEL_1) // Unit 1 Ch 1

// 모터 및 전류 제어 파라미터
#define PWM_FREQ_HZ             (25000)         // 25kHz PWM
#define OVERCURRENT_THRESHOLD_MA (1200)        // 과전류 차단 임계값: 1250mA

// ADC 설정 상수
#define ADC_OUTPUT_BIT_WIDTH    (SOC_ADC_DIGI_MAX_BITWIDTH) // 12-bit
#define ADC_READ_LEN            (256)

// 전역 변수 핸들
static mcpwm_cmpr_handle_t comparator_scp = NULL;
static mcpwm_cmpr_handle_t comparator_plate = NULL;
static adc_continuous_handle_t adc_handle = NULL;

static bool scp_inout_fault = false;
static bool plate_fault = false;

static QueueHandle_t dc_motor_msg = NULL;

void send_dc_motor_msg(void *message, uint32_t cmd)
{
	mt_message_t *msg = (mt_message_t *)message;
    msg->cmd = cmd;
    BaseType_t status = xQueueSend(dc_motor_msg, msg, pdMS_TO_TICKS(100));
    if (status == pdPASS) 
    {
//        ESP_LOGI(TAG, "[Sender %ld] transfer complete -> cmd %d ", msg->task_id, msg->cmd);
    } 
    else 
    {
//        ESP_LOGW(TAG, "[Sender %ld] queuse full transfer failed", msg->task_id);
    }
}

/**
 * 1. GPIO 초기화 (통합)
 */
static void init_system_gpios(void)
{
    ESP_LOGI(TAG, "%s", __func__);
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SCP_INOUT_SEL) | (1ULL << PLATE_SEL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    gpio_set_level(SCP_INOUT_SEL, MT_FORWARD_DIR);
    gpio_set_level(PLATE_SEL, MT_FORWARD_DIR);
}

/**
 * 2. MCPWM 및 타이머 설정 (하나의 그룹 및 타이머 자원 공유)
 */
static void init_mcpwm_system(void)
{
    ESP_LOGI(TAG, "%s", __func__);

    // 10MHz 기준 타이머 1개 생성
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000, 
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = 10000000 / PWM_FREQ_HZ, // 400 Ticks
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    // 동일 그룹에 오퍼레이터 생성
    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t oper_config = { .group_id = 0 };
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_config, &oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    // 공용 비교기 설정 구조체
    mcpwm_comparator_config_t cmpr_config = {.flags.update_cmp_on_tez = true};
    
    // ---------------- SCP_INOUT 모터 설정 ----------------
    mcpwm_gen_handle_t gen_scp = NULL;
    mcpwm_generator_config_t gen_cfg_scp = {.gen_gpio_num = SCP_INOUT_PWM};
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &gen_cfg_scp, &gen_scp));
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &cmpr_config, &comparator_scp));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen_scp, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_scp, MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator_scp, MCPWM_GEN_ACTION_LOW)));

    // ---------------- PLATE 모터 설정 ----------------
    mcpwm_gen_handle_t gen_plate = NULL;
    mcpwm_generator_config_t gen_cfg_plate = {.gen_gpio_num = PLATE_PWM};
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &gen_cfg_plate, &gen_plate));
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &cmpr_config, &comparator_plate));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen_plate, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_plate, MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator_plate, MCPWM_GEN_ACTION_LOW)));

    // 타이머 가동
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
}

/**
 * 3-1. SCP_INOUT 모터 제어
 */
static void set_motor_speed_scp_inout(int speed_pct)
{
    ESP_LOGI(TAG, "%s speed_pct %d ", __func__, speed_pct);
    if (scp_inout_fault) {
        mcpwm_comparator_set_compare_value(comparator_scp, 0);
        return;
    }
    uint32_t compare_val = (uint32_t)((abs(speed_pct) * 400) / 100);

    if (speed_pct > 0) {
        mcpwm_comparator_set_compare_value(comparator_scp, compare_val);
        gpio_set_level(SCP_INOUT_SEL, MT_FORWARD_DIR);
    } else if (speed_pct < 0) {
        mcpwm_comparator_set_compare_value(comparator_scp, compare_val);
        gpio_set_level(SCP_INOUT_SEL, MT_REVERSE_DIR);
    } else {
        mcpwm_comparator_set_compare_value(comparator_scp, 0);
    }
}

/**
 * 3-2. PLATE 모터 제어
 */
static void set_motor_speed_plate(int speed_pct)
{
//    ESP_LOGI(TAG, "%s speed_pct %d ", __func__, speed_pct);
    
    if (plate_fault) {
        mcpwm_comparator_set_compare_value(comparator_plate, 0);
        return;
    }
    uint32_t compare_val = (uint32_t)((abs(speed_pct) * 400) / 100);

    if (speed_pct > 0) {
        mcpwm_comparator_set_compare_value(comparator_plate, compare_val);
        gpio_set_level(PLATE_SEL, MT_FORWARD_DIR);
    } else if (speed_pct < 0) {
        mcpwm_comparator_set_compare_value(comparator_plate, compare_val);
        gpio_set_level(PLATE_SEL, MT_REVERSE_DIR);
    } else {
        mcpwm_comparator_set_compare_value(comparator_plate, 0);
    }
}

/**
 * 4. ADC 연속 모드 초기화 (두 채널을 멀티플렉싱 스캔하도록 통합)
 */
static void init_adc_continuous_system(void)
{
    ESP_LOGI(TAG, "%s", __func__);

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 2048,
        .conv_frame_size = ADC_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_continuous_config_t dig_config = {
        .sample_freq_hz = 40 * 1000, // 채널당 20kHz씩 할당하기 위해 전체 40kHz 샘플링 설정
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };

    // 2개의 채널 패턴 리스트 등록 (스캔 순서 정의)
    static adc_digi_pattern_config_t adc_patterns[2] = {
        {
            .atten = ADC_ATTEN_DB_12,
            .channel = SCP_INOUT_ADC_CH,
            .unit = ADC_UNIT_1,
            .bit_width = ADC_OUTPUT_BIT_WIDTH,
        },
        {
            .atten = ADC_ATTEN_DB_12,
            .channel = PLATE_ADC_CH,
            .unit = ADC_UNIT_1,
            .bit_width = ADC_OUTPUT_BIT_WIDTH,
        }
    };

    dig_config.pattern_num = 2;
    dig_config.adc_pattern = adc_patterns;

    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_config));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
}

/**
 * 5. 다중 모터 피드백 데이터 모니터링 및 과전류 차단 통합 태스크
 */
static void current_monitor_task_system(void *arg)
{
    uint8_t result[ADC_READ_LEN] = {0};
    uint32_t ret_num = 0;

    while (1) {
        esp_err_t ret = adc_continuous_read(adc_handle, result, ADC_READ_LEN, &ret_num, 10);
        if (ret == ESP_OK) {
            uint32_t scp_sum_voltage_mv = 0, scp_count = 0;
            uint32_t plate_sum_voltage_mv = 0, plate_count = 0;

            for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_DATA_BYTES_PER_CONV) {
                adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i];
                uint32_t chan = p->type2.channel;
                uint32_t data = p->type2.data & 0xFFF; // 12-bit 마스킹 데이터 추출 안정화

                if (chan == SCP_INOUT_ADC_CH) {
                    uint32_t mv = (data * 3100) / 4095;
                    scp_sum_voltage_mv += mv;
                    scp_count++;
                } else if (chan == PLATE_ADC_CH) {
                    uint32_t mv = (data * 3100) / 4095;
                    plate_sum_voltage_mv += mv;
                    plate_count++;
                }
            }

            // 1) SCP_INOUT 과전류 연산 및 처리
            if (scp_count > 0) {
                uint32_t avg_voltage_mv = scp_sum_voltage_mv / scp_count;
                uint32_t current_ma = avg_voltage_mv / 2;

                if (current_ma >= OVERCURRENT_THRESHOLD_MA && !scp_inout_fault) {
                    scp_inout_fault = true;
                    set_motor_speed_scp_inout(0);
                    ESP_LOGE(TAG, "[SCP FAULT] OVERCURRENT DETECTED! Halted. Current: %ld mA", current_ma);
                }
            }

            // 2) PLATE 과전류 연산 및 처리
            if (plate_count > 0) {
                uint32_t avg_voltage_mv = plate_sum_voltage_mv / plate_count;
                uint32_t current_ma = avg_voltage_mv / 2;

                if (current_ma >= OVERCURRENT_THRESHOLD_MA && !plate_fault) {
                    plate_fault = true;
                    set_motor_speed_plate(0);
                    ESP_LOGE(TAG, "[PLATE FAULT] OVERCURRENT DETECTED! Halted. Current: %ld mA", current_ma);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms 주기 모니터링
    }
}

void dc_motor_control_task(void *arg) {
    ESP_LOGI(TAG, "%s +", __func__);

    while (1) {
        mt_message_t msg = {0};
        if (xQueueReceive(dc_motor_msg, &msg, portMAX_DELAY) == pdPASS) {
//        	ESP_LOGI(TAG, "[Receiver %ld] transfer complete -> cmd %d ", msg.task_id, msg.cmd);
            switch((int)(msg.cmd))
            {
                case PLATE_FORWARD_CMD:
                    set_motor_speed_plate(99);
                    break;
                case PLATE_REVERSE_CMD:
                    set_motor_speed_plate(-99);
                    break;
                case PLATE_STOP_CMD: 
                    set_motor_speed_plate(0);
                    break;
                case SCP_INOUT_FORWARD_CMD:
                    set_motor_speed_scp_inout(-99);
                    break;
                case SCP_INOUT_REVERSE_CMD:
                    set_motor_speed_scp_inout(99);
                    break;
                case SCP_INOUT_STOP_CMD:
                    set_motor_speed_scp_inout(0);
                    break;
                
                default:
                break;
            }
        }
    }
//    ESP_LOGI(TAG, "%s -", __func__);
    vTaskDelete(NULL);
}

void dc_motor_init(void)
{
    ESP_LOGI(TAG, "%s", __func__);
	
    init_system_gpios();
    init_mcpwm_system();
    init_adc_continuous_system();

    dc_motor_msg = xQueueCreate(10, sizeof(mt_message_t));

    // 2. 모니터링 태스크 생성
    xTaskCreate(current_monitor_task_system, "current_monitor_task", 4096, NULL, 5, NULL);
    xTaskCreate(dc_motor_control_task, "dc_motor_control_task", 4096, NULL, 5, NULL);
}
