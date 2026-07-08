#include "main.h"

static const char *TAG = "STEP_MOTOR_CTRL";

///////////////////////////////////////////////////////////////////////
// MAIN COVER
#define MAIN_PWM_IN1        19             // 기존 STEP -> PWM 신호 1로 사용
#define MAIN_PWM_IN2        47             // 기존 DIR -> PWM 신호 2로 사용
#define MAIN_EN         	21             
#define MAIN_ADC_CH     	ADC_CHANNEL_3

///////////////////////////////////////////////////////////////////////
// WASTE COVER
#define WASTE_PWM_IN1       13             // 기존 STEP -> PWM 신호 1로 사용
#define WASTE_PWM_IN2       11             // 기존 DIR -> PWM 신호 2로 사용
#define WASTE_EN         	12             
#define WASTE_ADC_CH     	ADC_CHANNEL_9

// [35BYJ46 모터 속도 및 스텝 정의]
#define WASTE_TARGET_STEPS ((1020*2/8)+20)           	// add 20 steps, 90*2 = 180 degree, 90 degree : 127.5(1020/8) step
#define MAIN_TARGET_STEPS  ((1020/8)/2)           	// 45 degree
#define MOTOR_SPEED_MS     12             			// 1스텝당 시간 간격 (밀리초) - 낮을수록 빠름, 35BYJ46은 10~15ms가 안정적
#define MOTOR_SPEED_US     (MOTOR_SPEED_MS*1000) 	// 1스텝당 시간 간격 (밀리초) - 낮을수록 빠름, 35BYJ46은 10~15ms가 안정적

// [전류 제한 파라미터]
#define VREF               3.3f
#define ADC_RESOLUTION     4095.0f        
#define INA180_GAIN        20.0f          
#define R_SENSE            0.1f           
//#define MAX_CURRENT_LIMIT  0.5f           // 35BYJ46 소형 모터 보호를 위해 0.5A로 하향 조정
#define MAX_CURRENT_LIMIT  1.20f

///////////////////////////////////////////////////////////////////////
// scoop spin 
#define PIN_DRV_STEP          GPIO_NUM_6
#define PIN_DRV_DIR           GPIO_NUM_20
#define PIN_DRV_EN            GPIO_NUM_5
#define PIN_DRV_NSLEEP        GPIO_NUM_46
#define PIN_INA_OUT_ADC       ADC_CHANNEL_6

// 시스템 설정값
#define SHUNT_RESISTOR_OHM    	0.1f          // 션트 저항값 (100mOhm)
#define INA180A1_GAIN        	20.0f          
#define ADC_VREF_MV           	3300.0f       // ADC 기준 전압 (3.3V)
#define ADC_MAX_VAL           	4095.0f       // 12비트 ADC 최대값

// [모터 사양 설정] 90도 회전을 위한 스텝 수 계산
#define MOTOR_STEP_ANGLE      1.8f          // 모터의 기본 스텝 각도 (보통 1.8도)
#define MICROSTEPPING         48//32             // DRV8425의 마이크로스텝 설정값 (예: 1/8 스텝)
// (90도 / 1.8도) * 8 = 400 스텝 필요
#define TARGET_STEPS_70_DEG   ((uint32_t)((70.0f / MOTOR_STEP_ANGLE) * MICROSTEPPING))
#define TARGET_STEPS_80_DEG   ((uint32_t)((80.0f / MOTOR_STEP_ANGLE) * MICROSTEPPING))
#define TARGET_STEPS_90_DEG   ((uint32_t)((90.0f / MOTOR_STEP_ANGLE) * MICROSTEPPING))
#define TARGET_STEPS_150_DEG   ((uint32_t)((150.0f / MOTOR_STEP_ANGLE) * MICROSTEPPING))
#define TARGET_STEPS_180_DEG   ((uint32_t)((180.0f / MOTOR_STEP_ANGLE) * MICROSTEPPING))
#define TARGET_STEPS_270_DEG   ((uint32_t)((270.0f / MOTOR_STEP_ANGLE) * MICROSTEPPING))
#define TARGET_STEPS_360_DEG   ((uint32_t)((360.0f / MOTOR_STEP_ANGLE) * MICROSTEPPING))

// MCPWM 및 ADC 핸들 선언
static mcpwm_timer_handle_t   mcpwm_timer = NULL;
static mcpwm_cmpr_handle_t    mcpwm_cmpr = NULL;
static adc_oneshot_unit_handle_t adc1_handle;

static int g_step_mt_select = MAIN_COVER_MOTOR; // 0: main, 1: waste

// 전역 상태 변수
volatile uint32_t step_counter = 0;
volatile uint32_t target_steps = 0;
volatile bool mt_scp_spin_run = false;

static QueueHandle_t step_motor_msg = NULL;

void send_step_motor_msg(void *message, uint32_t cmd)
{
	mt_message_t *msg = (mt_message_t *)message;
    msg->cmd = cmd;
    BaseType_t status = xQueueSend(step_motor_msg, msg, pdMS_TO_TICKS(100));
    if (status == pdPASS) 
    {
//        ESP_LOGI(TAG, "[Sender %ld] transfer complete -> cmd %d ", msg->task_id, msg->cmd);
    } 
    else 
    {
//        ESP_LOGW(TAG, "[Sender %ld] queuse full transfer failed", msg->task_id);
    }
}

// 1. 하드웨어 타이머 및 GPIO 설정
static void init_peripherals(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << WASTE_PWM_IN1) | (1ULL << WASTE_PWM_IN2) | (1ULL << WASTE_EN) |
			        	(1ULL << MAIN_PWM_IN1) | (1ULL << MAIN_PWM_IN2) | (1ULL << MAIN_EN) |
                        (1ULL << PIN_DRV_STEP) | (1ULL << PIN_DRV_DIR) | (1ULL << PIN_DRV_EN) | (1ULL << PIN_DRV_NSLEEP),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);

    // 초기 상태: 모터 드라이버 비활성화 (High 또는 Low는 드라이버 사양에 맞게 조절)
    gpio_set_level(WASTE_EN, 0);    
    gpio_set_level(MAIN_EN, 0);    
    gpio_set_level(PIN_DRV_EN, 0);  // DRV8425 활성화 (High Active)
    gpio_set_level(PIN_DRV_NSLEEP, 0);  // DRV8425 활성화 (High Active)
    gpio_set_level(PIN_DRV_DIR, 1); // 초기 정방향

    // PWM을 가할 IN1, IN2 핀 설정 (GPIO 모드로 초기 가동 후 필요시 펄스 출력)
    gpio_reset_pin(WASTE_PWM_IN1);
    gpio_reset_pin(WASTE_PWM_IN2);
    gpio_set_direction(WASTE_PWM_IN1, GPIO_MODE_OUTPUT);
    gpio_set_direction(WASTE_PWM_IN2, GPIO_MODE_OUTPUT);
    gpio_set_level(WASTE_PWM_IN1, 0);
    gpio_set_level(WASTE_PWM_IN2, 0);

    gpio_reset_pin(MAIN_PWM_IN1);
    gpio_reset_pin(MAIN_PWM_IN2);
    gpio_set_direction(MAIN_PWM_IN1, GPIO_MODE_OUTPUT);
    gpio_set_direction(MAIN_PWM_IN2, GPIO_MODE_OUTPUT);
    gpio_set_level(MAIN_PWM_IN1, 0);
    gpio_set_level(MAIN_PWM_IN2, 0);

    // ADC1 초기화 (v5.5 규격)
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, WASTE_ADC_CH, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, MAIN_ADC_CH, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, PIN_INA_OUT_ADC, &config));
}


static int motor_enable(uint32_t enable)
{
	switch(g_step_mt_select)
	{
		case MAIN_COVER_MOTOR:
			gpio_set_level(MAIN_EN, enable);    
			break;
		case WASTE_COVER_MOTOR:
			gpio_set_level(WASTE_EN, enable);    
			break;
		case SCP_SPIN_MOTOR:
			gpio_set_level(PIN_DRV_EN, enable);    
			gpio_set_level(PIN_DRV_NSLEEP, enable);    
			break;
		default:
			break;
	}
	return 0;
}

// 5. ADC를 통한 전류 피드백 모니터링 함수
static float read_current_ma(int mode) {
    int adc_raw = 0;
    if(mode == MAIN_COVER_MOTOR)
    {
	    if (adc_oneshot_read(adc1_handle, MAIN_ADC_CH, &adc_raw) == ESP_OK) {
	        // 전압 계산 (mV)
	        float voltage_mv = ((float)adc_raw / ADC_MAX_VAL) * ADC_VREF_MV;
	        // INA180 증폭 전 원래 션트 저항 전압 (V)
	        float v_shunt = (voltage_mv / 1000.0f) / INA180A1_GAIN;
	        // 옴의 법칙 (I = V / R) 적용하여 전류(A) 계산
	        float current_a = v_shunt / SHUNT_RESISTOR_OHM;
	        return current_a;
	    }
    }
    else if(mode == WASTE_COVER_MOTOR)
    {
	    if (adc_oneshot_read(adc1_handle, WASTE_ADC_CH, &adc_raw) == ESP_OK) {
	        // 전압 계산 (mV)
	        float voltage_mv = ((float)adc_raw / ADC_MAX_VAL) * ADC_VREF_MV;
	        // INA180 증폭 전 원래 션트 저항 전압 (V)
	        float v_shunt = (voltage_mv / 1000.0f) / INA180A1_GAIN;
	        // 옴의 법칙 (I = V / R) 적용하여 전류(A) 계산
	        float current_a = v_shunt / SHUNT_RESISTOR_OHM;
	        return current_a;
        }
	}
    else if(mode == SCP_SPIN_MOTOR)
    {
	    if (adc_oneshot_read(adc1_handle, PIN_INA_OUT_ADC, &adc_raw) == ESP_OK) {
	        // 전압 계산 (mV)
	        float voltage_mv = ((float)adc_raw / ADC_MAX_VAL) * ADC_VREF_MV;
	        // INA180 증폭 전 원래 션트 저항 전압 (V)
	        float v_shunt = (voltage_mv / 1000.0f) / INA180A1_GAIN;
	        // 옴의 법칙 (I = V / R) 적용하여 전류(A) 계산
	        float current_a = v_shunt / SHUNT_RESISTOR_OHM;
	        return current_a;
	    }
	}
    return 0.0f;
}

static int g_current_steps = 0;
static int g_current_phase = 0;
static int g_current_direction = 0;
static bool mt_main_waste_run = false;
static esp_timer_handle_t g_motor_timer = NULL;

static bool is_overcurrent_tripped(int mode) {
    float current = read_current_ma(mode);
    
//    ESP_LOGI(TAG, "cc     %.1fmA", current);
//    ESP_LOGI(TAG, "Driving... Current: %.1fmA, Progress: %lu/%lu", current, g_current_steps, WASTE_TARGET_STEPS);

    if (current > MAX_CURRENT_LIMIT) {
        motor_enable(0);
        ESP_LOGE(TAG, "[TRIP] Overcurrent Detected: %.2f A", current);
        return true;
    }
    return false;
}

static void main_waste_stop(int mt) {
    // 1. 모터 구동 플래그 해제
    mt_main_waste_run = false;
    
    // 2. 하드웨어 타이머 정지 (실행 중인 경우)
    if (g_motor_timer != NULL) {
        esp_timer_stop(g_motor_timer);
    }
    
    // 3. 전력 차단 및 과열 방지를 위해 모든 제어 핀 LOW 설정
    if(mt)
    {
	    gpio_set_level(WASTE_PWM_IN1, 0);
    	gpio_set_level(WASTE_PWM_IN2, 0);
	}
    else
    {
        gpio_set_level(MAIN_PWM_IN1, 0);
        gpio_set_level(MAIN_PWM_IN2, 0);
    }
	motor_enable(0);    
    // 4. 상태 변수 초기화
    g_current_steps = 0;
}

#if 1
// 2,500us(2.5ms) 마다 하드웨어 인터럽트에 의해 실행되는 콜백 함수
static void motor_timer_callback(void* arg) {
    if (!mt_main_waste_run) return;

	// generate pulse
	switch(g_current_phase)
	{
		case 0:
			if(g_current_direction)
			{
				if(g_step_mt_select == MAIN_COVER_MOTOR)
				{
					gpio_set_level(MAIN_PWM_IN1, 1);		
					gpio_set_level(MAIN_PWM_IN2, 0);
				}
				else
				{
					gpio_set_level(WASTE_PWM_IN1, 1);		
					gpio_set_level(WASTE_PWM_IN2, 0);
				}
			}
			else
			{
				if(g_step_mt_select == MAIN_COVER_MOTOR)
				{
                    gpio_set_level(MAIN_PWM_IN2, 1);       
                    gpio_set_level(MAIN_PWM_IN1, 0);
				}
				else
				{
					gpio_set_level(WASTE_PWM_IN2, 1);		
					gpio_set_level(WASTE_PWM_IN1, 0);
				}
			}
		break;
		case 1:
			if(g_current_direction)
			{
				if(g_step_mt_select == MAIN_COVER_MOTOR)
				{
                    gpio_set_level(MAIN_PWM_IN1, 1);       
                    gpio_set_level(MAIN_PWM_IN2, 1);
				}
				else
				{
					gpio_set_level(WASTE_PWM_IN1, 1);		
					gpio_set_level(WASTE_PWM_IN2, 1);
				}
			}
			else
			{
				if(g_step_mt_select == MAIN_COVER_MOTOR)
				{
                    gpio_set_level(MAIN_PWM_IN2, 1);       
                    gpio_set_level(MAIN_PWM_IN1, 1);
				}
				else
				{
					gpio_set_level(WASTE_PWM_IN2, 1);		
					gpio_set_level(WASTE_PWM_IN1, 1);
				}
			}
		break;
		case 2:
			if(g_current_direction)
			{
				if(g_step_mt_select == MAIN_COVER_MOTOR)
				{
                    gpio_set_level(MAIN_PWM_IN1, 0);       
                    gpio_set_level(MAIN_PWM_IN2, 1);
				}
				else
				{
					gpio_set_level(WASTE_PWM_IN1, 0);		
					gpio_set_level(WASTE_PWM_IN2, 1);
				}
			}
			else
			{
				if(g_step_mt_select == MAIN_COVER_MOTOR)
				{
                    gpio_set_level(MAIN_PWM_IN2, 0);       
                    gpio_set_level(MAIN_PWM_IN1, 1);
				}
				else
				{
					gpio_set_level(WASTE_PWM_IN2, 0);		
					gpio_set_level(WASTE_PWM_IN1, 1);
				}
			}
		break;
		case 3:
			if(g_current_direction)
			{
				if(g_step_mt_select == MAIN_COVER_MOTOR)
				{
                    gpio_set_level(MAIN_PWM_IN1, 0);       
                    gpio_set_level(MAIN_PWM_IN2, 0);
				}
				else
				{
					gpio_set_level(WASTE_PWM_IN1, 0);		
					gpio_set_level(WASTE_PWM_IN2, 0);
				}
			}
			else
			{
				if(g_step_mt_select == MAIN_COVER_MOTOR)
				{
                    gpio_set_level(MAIN_PWM_IN2, 0);       
                    gpio_set_level(MAIN_PWM_IN1, 0);
				}
				else
				{
					gpio_set_level(WASTE_PWM_IN2, 0);		
					gpio_set_level(WASTE_PWM_IN1, 0);
				}
			}
		break;
		default:
		break;
	}
	g_current_phase++;
	if(g_current_phase > 3)
	{
		g_current_phase = 0;
		g_current_steps--;
        if (g_current_steps <= 0) {
            mt_main_waste_run = false;
            esp_timer_stop(g_motor_timer); // 목표 스텝 도달 시 타이머 정지
            motor_enable(0);
        }
	}
}

static bool main_waste_move(int steps, int dir) {
    g_current_steps = steps;
	g_current_phase = 0;
    mt_main_waste_run = true;
    g_current_direction = dir;
    motor_enable(1);
    
    if (g_motor_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = &motor_timer_callback,
            .name = "motor_pulse_timer"
        };
        esp_timer_create(&timer_args, &g_motor_timer);
    }
    esp_timer_start_periodic(g_motor_timer, MOTOR_SPEED_US);
#if 0
    while (mt_main_waste_run) {
        if (is_overcurrent_tripped(g_step_mt_select)) {
            mt_main_waste_run = false;
            esp_timer_stop(g_motor_timer);
            motor_enable(0);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }

    // 과전류 등으로 도중에 멈췄는지 체크
    if (g_current_steps > 0) {
        return false; // hard_lock_state 구동용
    }
#endif
    return true;
}

#else

static const int phase_table[4][2] = {
    {1, 0}, // Phase 0
    {1, 1}, // Phase 1
    {0, 1}, // Phase 2
    {0, 0}  // Phase 3
};

static void motor_timer_callback(void* arg) 
{
    if (!mt_main_waste_run) return;

    // 1. 현재 선택된 모터의 GPIO 핀 결정
    int gpio_in1 = (g_step_mt_select == MAIN_COVER_MOTOR) ? MAIN_PWM_IN1 : WASTE_PWM_IN1;
    int gpio_in2 = (g_step_mt_select == MAIN_COVER_MOTOR) ? MAIN_PWM_IN2 : WASTE_PWM_IN2;

    // 2. 현재 Phase 테이블 값 출력
    gpio_set_level(gpio_in1, phase_table[g_current_phase][0]);
    gpio_set_level(gpio_in2, phase_table[g_current_phase][1]);

    // 3. 1스텝 소비 및 종료 조건 체크 (매 펄스마다 감소)
    g_current_steps--;
    if (g_current_steps <= 0) {
        mt_main_waste_run = false;
        esp_timer_stop(g_motor_timer);
        motor_enable(0);
        return;
    }

    // 4. 방향에 따른 다음 Phase 계산
    if (g_current_direction) {
        g_current_phase = (g_current_phase + 1) % 4; // 정방향 증가
    } else {
        g_current_phase = (g_current_phase - 1 + 4) % 4; // 역방향 감소
    }
}

static bool main_waste_move(int steps, int dir) 
{
    if (steps <= 0) return true;

    g_current_steps = steps;
    g_current_direction = dir;
    g_current_phase = 0;
    mt_main_waste_run = true;

    motor_enable(1);

    // 타이머가 없다면 최초 1회 생성
    if (g_motor_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = &motor_timer_callback,
            .name = "motor_pulse_timer"
        };
        esp_timer_create(&timer_args, &g_motor_timer);
    } else {
        // 기존에 작동 중이던 타이머 안전하게 정지
        esp_timer_stop(g_motor_timer);
    }

    // 타이머 시작 (MOTOR_SPEED_US = 2500)
    esp_timer_start_periodic(g_motor_timer, MOTOR_SPEED_US);

    // 모터 구동 완료 및 과전류 감시 루프
    while (mt_main_waste_run) {
        if (is_overcurrent_tripped(g_step_mt_select)) {
            mt_main_waste_run = false;
            esp_timer_stop(g_motor_timer);
            motor_enable(0);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // 목표 스텝을 다 채우지 못하고 멈췄다면 false 반환
    return (g_current_steps <= 0);
}
#endif

// 1. MCPWM 인터럽트 핸들러 (스텝 카운팅용 고속 ISR - on_full 전용)
// IRAM_ATTR 속성을 부여하여 플래시 메모리가 아닌 SRAM에서 즉시 실행되도록 실시간성 강화
static bool IRAM_ATTR mcpwm_timer_full_cb(mcpwm_timer_handle_t timer, const mcpwm_timer_event_data_t *edata, void *user_ctx) {
    if (mt_scp_spin_run) {
        step_counter++; // 펄스가 피크(on_full)에 도달할 때마다 스텝 카운트 증가
        
        if (step_counter >= target_steps) {
            // 목표 스텝(각도) 도달 시, 현재 주기를 마지막으로 하드웨어 타이머 정지 명령
            mcpwm_timer_start_stop(timer, MCPWM_TIMER_STOP_EMPTY);
            motor_enable(0);
            mt_scp_spin_run = false;
        }
    }
    // 기본적으로 다른 FreeRTOS 태스크로의 즉각적인 컨텍스트 스위칭이 필요 없으므로 false 반환
    return false; 
}

// 2. MCPWM 초기화 (STEP 펄스 생성용)
static void init_mcpwm_step_generator(void) {
    ESP_LOGI(TAG, "%%s +", __func__);

    // 타이머 기본 설정: 1MHz 분해능, Up-Count 모드, 주기 2000us (500Hz)
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, 
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = 2000,     
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &mcpwm_timer));

    // 오퍼레이터 생성 및 타이머 바인딩
    mcpwm_operator_config_t operator_config = { .group_id = 0 };
    mcpwm_oper_handle_t oper = NULL;
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, mcpwm_timer));

    // 비교기(Comparator) 설정 및 50% 듀티 사이클 지정
    mcpwm_comparator_config_t comparator_config = { .flags.update_cmp_on_tez = true };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &mcpwm_cmpr));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(mcpwm_cmpr, 1000)); 

    // 지정된 STEP 핀(GPIO 6)으로 제너레이터 생성
    mcpwm_generator_config_t generator_config = { .gen_gpio_num = PIN_DRV_STEP };
    mcpwm_gen_handle_t generator = NULL;
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));

    // 파형 동작 정의: 0일 때 High, 비교값 일치 시 Low (사각파 생성)
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_timer_event(generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH),
        MCPWM_GEN_TIMER_EVENT_ACTION_END()));
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, mcpwm_cmpr, MCPWM_GEN_ACTION_LOW),
        MCPWM_GEN_COMPARE_EVENT_ACTION_END()));

    // ?? [변경 핵심] 타이머 카운터가 최댓값(Peak)에 도달할 때 인터럽트를 발생시키도록 콜백 등록
    mcpwm_timer_event_callbacks_t cbs = {
        .on_full  = mcpwm_timer_full_cb,  // 앞서 정의한 on_full 전용 고속 ISR 매핑
        .on_empty = NULL,
        .on_stop  = NULL
    };
    
    // 필수 규칙: 콜백 등록은 반드시 mcpwm_timer_enable()을 실행하기 '전'에 완료해야 합니다.
    ESP_ERROR_CHECK(mcpwm_timer_register_event_callbacks(mcpwm_timer, &cbs, NULL));
    
    // MCPWM 타이머 하드웨어 레이어 활성화
    ESP_ERROR_CHECK(mcpwm_timer_enable(mcpwm_timer));
}

static void scoop_spin_stop(bool immediate) {
    ESP_LOGI("MOTOR_CTRL", "Motor stop requested. Mode: %s", immediate ? "IMMEDIATE" : "SAFE");

    // 1. 이미 멈춰있는 상태라면 중복 처리 방지
    if (!mt_scp_spin_run) {
        return;
    }

    // 2. 하드웨어 타이머 정지 명령 및 모터 드라이버 비활성화
    if (immediate) {
        // 즉시 정지: 타이머를 완전히 정지시키고 드라이버를 즉시 끕니다.
        mcpwm_timer_start_stop(mcpwm_timer, MCPWM_TIMER_STOP_EMPTY);
        motor_enable(0);
    } else {
        // 안전 정지: 현재 주기가 끝날 때(EMPTY 상태가 될 때) 타이머가 멈추도록 예약합니다.
        mcpwm_timer_start_stop(mcpwm_timer, MCPWM_TIMER_STOP_EMPTY);
        
        // 타이머가 완전히 멈출 때까지 잠시 대기 (최대 현재 1주기 시간인 2ms 소요)
        // 안전 정지 시에는 드라이버를 바로 끄면 펄스가 잘릴 수 있으므로 타이머가 멈춘 후 끕니다.
        uint32_t timeout = 0;
        while (mt_scp_spin_run && timeout < 10) { 
            vTaskDelay(pdMS_TO_TICKS(1));
            timeout++;
        }
        motor_enable(0);
    }

    // 3. 소프트웨어 상태 변수 최종 정리 및 리셋
    mt_scp_spin_run = false;
    step_counter = 0;
    target_steps = 0;

    ESP_LOGI("MOTOR_CTRL", "Motor cleanup finished successfully.");
}

static void scoop_spin_move(uint32_t steps, uint8_t direction) {
    gpio_set_level(PIN_DRV_DIR, direction);
    
    step_counter = 0;
    target_steps = steps;
    mt_scp_spin_run = true;

    motor_enable(1);

    ESP_LOGI(TAG, "Motor Start: %s, Target Steps: %lu", direction ? "FORWARD" : "BACKWARD", steps);

    // 하드웨어 타이머 가동
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(mcpwm_timer, MCPWM_TIMER_START_NO_STOP));
#if 0   
    while (mt_scp_spin_run) {
        float current_ma = read_current_ma(SCP_SPIN_MOTOR);
        ESP_LOGI(TAG, "Driving... Current: %.1fmA, Progress: %lu/%lu", current_ma, step_counter, target_steps);
        if (is_overcurrent_tripped(SCP_SPIN_MOTOR)) {
            scoop_spin_stop(true); 	// immediate terminate
            ESP_LOGE(TAG, "Overcurrent Tripped! Motor Stopped Safety.");
            break;
        }
        
        // 너무 잦은 로그 방지 및 컨텍스트 스위칭 유도
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
    
    ESP_LOGI(TAG, "Motor Stopped Target Reached.");
#endif
}


static int step_mode = STEP_IDLE_MODE;
static SemaphoreHandle_t step_mutex;

static int set_step_mode(int mode)
{
    if (xSemaphoreTake(step_mutex, portMAX_DELAY) == pdTRUE) 
    {
        step_mode = mode;
        xSemaphoreGive(step_mutex);
    }
    return 0;
}

static void step_motor_monitor(void *arg)
{
    ESP_LOGI(TAG, "%s +", __func__);
    while (1)
    {
    	switch(step_mode)
    	{
			case STEP_IDLE_MODE:
			break;
            case MAIN_WASTE_WAIT_MODE:
                if(mt_main_waste_run) 
                {
                    if (is_overcurrent_tripped(g_step_mt_select)) {
                        mt_main_waste_run = false;
                        esp_timer_stop(g_motor_timer);
                        motor_enable(0);
                        set_step_mode(STEP_IDLE_MODE);
                        break;
                    }
                }
                else
                {
					set_step_mode(STEP_IDLE_MODE);
                }
            break;
            case SCP_SPIN_WAIT_MODE:
                if(mt_scp_spin_run) 
                {
//                    float current_ma = read_current_ma(SCP_SPIN_MOTOR);
//                    ESP_LOGI(TAG, "Driving... Current: %.1fmA, Progress: %lu/%lu", current_ma, step_counter, target_steps);
                    if (is_overcurrent_tripped(SCP_SPIN_MOTOR)) {
                        scoop_spin_stop(true);  // immediate terminate
                        set_step_mode(STEP_IDLE_MODE);
                        ESP_LOGE(TAG, "Overcurrent Tripped! Motor Stopped Safety.");
                        break;
                    }
                }
                else
                {
					set_step_mode(STEP_IDLE_MODE);
                }
            break;

			default:
			break;
		}
		vTaskDelay(pdMS_TO_TICKS(10));
    }
//    ESP_LOGI(TAG, "%s -", __func__);
	vTaskDelete(NULL);
}

bool get_step_motor_run(step_motor_t mt)
{
	bool ret = false;
	if(mt == MAIN_COVER_MOTOR)
	{
		ret = mt_main_waste_run;
	}
	else if(mt == WASTE_COVER_MOTOR)
	{
		ret = mt_main_waste_run;
	}
	else if(mt == SCP_SPIN_MOTOR)
	{
		ret = mt_scp_spin_run;
	}
	return ret;
}

void step_motor_cmd_task(void *arg) 
{
    ESP_LOGI(TAG, "%s +", __func__);
    uint32_t ang;
    while (1) {
        mt_message_t msg = {0};
        if (xQueueReceive(step_motor_msg, &msg, portMAX_DELAY) == pdPASS) {
//          ESP_LOGI(TAG, "[Receiver %ld] transfer complete -> cmd %d ", msg.task_id, msg.cmd);
            switch((int)(msg.cmd))
            {
                case MAIN_FORWARD_CMD:
                	g_step_mt_select = MAIN_COVER_MOTOR;
                    main_waste_move(MAIN_TARGET_STEPS, 0);
                    set_step_mode(MAIN_WASTE_WAIT_MODE);
                    break;
                case MAIN_REVERSE_CMD:
                	g_step_mt_select = MAIN_COVER_MOTOR;
                    main_waste_move(MAIN_TARGET_STEPS, 1);
                    set_step_mode(MAIN_WASTE_WAIT_MODE);
                    break;
                case MAIN_STOP_CMD: 
                	g_step_mt_select = MAIN_COVER_MOTOR;
                	main_waste_stop(0);
                	set_step_mode(STEP_IDLE_MODE);
                    break;
                case WASTE_FORWARD_CMD:
                	g_step_mt_select = WASTE_COVER_MOTOR;
                    main_waste_move(WASTE_TARGET_STEPS, 1);
                    set_step_mode(MAIN_WASTE_WAIT_MODE);
                    break;
                case WASTE_REVERSE_CMD:
                	g_step_mt_select = WASTE_COVER_MOTOR;
                    main_waste_move(WASTE_TARGET_STEPS, 0);
                    set_step_mode(MAIN_WASTE_WAIT_MODE);
                    break;
                case WASTE_STOP_CMD: 
                	g_step_mt_select = WASTE_COVER_MOTOR;
                	main_waste_stop(1);
                	set_step_mode(STEP_IDLE_MODE);
                    break;
                case SCP_SPIN_FORWARD_CMD:
                	g_step_mt_select = SCP_SPIN_MOTOR;
                	ang = msg.angle;
                	if(ang == 70)
                	{
                        scoop_spin_move(TARGET_STEPS_70_DEG, 1);
                	}
                	else if(ang == 80)
                	{
                        scoop_spin_move(TARGET_STEPS_80_DEG, 1);
                	}
                	else if(ang == 90)
                	{
                        scoop_spin_move(TARGET_STEPS_90_DEG, 1);
                	}
                	else if(ang == 150)
                	{
                        scoop_spin_move(TARGET_STEPS_150_DEG, 1);
                	}
                	else if(ang == 180)
                	{
                        scoop_spin_move(TARGET_STEPS_180_DEG, 1);
                	}
                	else if(ang == 270)
                	{
                        scoop_spin_move(TARGET_STEPS_270_DEG, 1);
                	}
                	else if(ang == 360)
                	{
                        scoop_spin_move(TARGET_STEPS_360_DEG, 1);
                	}
                	set_step_mode(SCP_SPIN_WAIT_MODE);
//                    ESP_LOGI(TAG, "SCP_SPIN_FORWARD_CMD 90 deg CW. Current Peak: %.1fmA. Holding for 5s...", read_current_ma(SCP_SPIN_MOTOR));
                    break;
                case SCP_SPIN_REVERSE_CMD:
                	g_step_mt_select = SCP_SPIN_MOTOR;
                	ang = msg.angle;
                	if(ang == 70)
                	{
                        scoop_spin_move(TARGET_STEPS_70_DEG, 0);
                	}
                	else if(ang == 80)
                	{
                        scoop_spin_move(TARGET_STEPS_80_DEG, 0);
                	}
                	else if(ang == 90)
                	{
                        scoop_spin_move(TARGET_STEPS_90_DEG, 0);
                	}
                	else if(ang == 150)
                	{
                        scoop_spin_move(TARGET_STEPS_150_DEG, 0);
                	}
                	else if(ang == 180)
                	{
                        scoop_spin_move(TARGET_STEPS_180_DEG, 0);
                	}
                	else if(ang == 270)
                	{
                        scoop_spin_move(TARGET_STEPS_270_DEG, 0);
                	}
                	else if(ang == 360)
                	{
                        scoop_spin_move(TARGET_STEPS_360_DEG, 0);
                	}
                	set_step_mode(SCP_SPIN_WAIT_MODE);
//                    ESP_LOGI(TAG, "SCP_SPIN_REVERSE_CMD 90 deg CCW. Current Peak: %.1fmA. Holding for 5s...", read_current_ma(SCP_SPIN_MOTOR));
                    break;
                case SCP_SPIN_STOP_CMD: 
                	g_step_mt_select = SCP_SPIN_MOTOR;
                	scoop_spin_stop(false); // safe terminate
                	set_step_mode(STEP_IDLE_MODE);
                    break;
                
                default:
                break;
            }
        }
    }
//    ESP_LOGI(TAG, "%s -", __func__);
    vTaskDelete(NULL);
}


void step_motor_init(void) 
{
	ESP_LOGI(TAG, "%s", __func__);
	
    step_mutex = xSemaphoreCreateMutex();

	// initialize waste cover
    init_peripherals();

    // initialize scoop spin 
    init_mcpwm_step_generator();

    step_motor_msg = xQueueCreate(10, sizeof(mt_message_t));

    xTaskCreate(step_motor_monitor, "step_motor_monitor", 4096, NULL, 5, NULL);

//    xTaskCreate(step_motor_cmd_task, "step_motor_cmd_task", 4096, NULL, 5, NULL);
    xTaskCreatePinnedToCore(step_motor_cmd_task, "step_motor_cmd_task", 4096, NULL, 5, NULL, 1);
}
