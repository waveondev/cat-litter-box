#include "main.h"

static const char *TAG = "STEP_MOTOR_CTRL";

///////////////////////////////////////////////////////////////////////
// MAIN COVER
#define MAIN_PWM_IN1        GPIO_NUM_19
#define MAIN_PWM_IN2        GPIO_NUM_47
#define MAIN_EN         	GPIO_NUM_21             

///////////////////////////////////////////////////////////////////////
// WASTE COVER
#define WASTE_PWM_IN1       GPIO_NUM_13
#define WASTE_PWM_IN2       GPIO_NUM_11
#define WASTE_EN         	GPIO_NUM_12             

#define WASTE_TARGET_STEPS (1020+20)           	// 360 degree : 2040 steps, 180 deree: 1020, add 20 steps
#define MAIN_TARGET_STEPS  (1020/2)           	// 45 degree
#define MOTOR_SPEED_MS     12             			// 1스텝당 시간 간격 (밀리초) - 낮을수록 빠름, 35BYJ46은 10~15ms가 안정적
#define MOTOR_SPEED_US     (MOTOR_SPEED_MS*1000) 	// 1스텝당 시간 간격 (밀리초) - 낮을수록 빠름, 35BYJ46은 10~15ms가 안정적

///////////////////////////////////////////////////////////////////////
// scoop spin 
#define PIN_DRV_STEP          GPIO_NUM_6
#define PIN_DRV_DIR           GPIO_NUM_20
#define PIN_DRV_EN            GPIO_NUM_5
#define PIN_DRV_NSLEEP        GPIO_NUM_46

// [모터 사양 설정] 90도 회전을 위한 스텝 수 계산
#define MOTOR_STEP_ANGLE      1.8f          	// 모터의 기본 스텝 각도 (보통 1.8도)
#define MICROSTEPPING         51//32			// DRV8425의 마이크로스텝 설정값 (예: 1/8 스텝)

#if 0
// (90도 / 1.8도) * 8 = 400 스텝 필요
#define TARGET_STEPS_70_DEG   ((uint32_t)((70.0f / MOTOR_STEP_ANGLE) * MICROSTEPPING))
#define TARGET_STEPS_80_DEG   ((uint32_t)((80.0f / MOTOR_STEP_ANGLE) * MICROSTEPPING))
#define TARGET_STEPS_90_DEG   ((uint32_t)((90.0f / MOTOR_STEP_ANGLE) * MICROSTEPPING))
#define TARGET_STEPS_150_DEG   ((uint32_t)((150.0f / MOTOR_STEP_ANGLE) * MICROSTEPPING))
#define TARGET_STEPS_180_DEG   ((uint32_t)((180.0f / MOTOR_STEP_ANGLE) * MICROSTEPPING))
#define TARGET_STEPS_270_DEG   ((uint32_t)((270.0f / MOTOR_STEP_ANGLE) * MICROSTEPPING))
#define TARGET_STEPS_360_DEG   ((uint32_t)((360.0f / MOTOR_STEP_ANGLE) * MICROSTEPPING))
#else
#define DEG_TO_STEPS(deg)  ((uint32_t)(((float)(deg) / MOTOR_STEP_ANGLE) * MICROSTEPPING))
#endif
static mcpwm_timer_handle_t   mcpwm_timer = NULL;
static mcpwm_cmpr_handle_t    mcpwm_cmpr = NULL;

volatile uint32_t scpspin_cnt = 0;
volatile uint32_t scpspin_steps = 0;
volatile bool scpspin_motor_run = false;

static int main_motor_steps = 0;
static int main_motor_phase = 0;
static int main_motor_dir = 0;
volatile static bool main_motor_run = false;
static esp_timer_handle_t main_motor_timer = NULL;

static int waste_motor_steps = 0;
static int waste_motor_phase = 0;
static int waste_motor_dir = 0;
volatile static bool waste_motor_run = false;
static esp_timer_handle_t waste_motor_timer = NULL;

static const int phase_table[4][2] = {
    {1, 0}, // Phase 0
    {1, 1}, // Phase 1
    {0, 1}, // Phase 2
    {0, 0}  // Phase 3
};

static QueueHandle_t main_motor_msg = NULL;
static QueueHandle_t waste_motor_msg = NULL;
static QueueHandle_t scpspin_motor_msg = NULL;

static int main_step_mode = MAIN_IDLE_MODE;
static int waste_step_mode = WASTE_IDLE_MODE;
static int scpspin_step_mode = SCPSPIN_IDLE_MODE;
static SemaphoreHandle_t main_mutex;
static SemaphoreHandle_t waste_mutex;
static SemaphoreHandle_t scpspin_mutex;

void send_main_motor_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout)
{
	mt_message_t *msg = (mt_message_t *)message;
    msg->cmd = cmd;
    msg->angle = angle;
    msg->direction = dir;
    msg->timeout = timeout;
    BaseType_t status = xQueueSend(main_motor_msg, msg, pdMS_TO_TICKS(100));
    if (status == pdPASS) 
    {
//        ESP_LOGI(TAG, "[Sender %ld] transfer complete -> cmd %d ", msg->task_id, msg->cmd);
    } 
    else 
    {
//        ESP_LOGW(TAG, "[Sender %ld] queuse full transfer failed", msg->task_id);
    }
}

void send_waste_motor_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout)
{
	mt_message_t *msg = (mt_message_t *)message;
    msg->cmd = cmd;
    msg->angle = angle;
    msg->direction = dir;
    msg->timeout = timeout;
    BaseType_t status = xQueueSend(waste_motor_msg, msg, pdMS_TO_TICKS(100));
    if (status == pdPASS) 
    {
//        ESP_LOGI(TAG, "[Sender %ld] transfer complete -> cmd %d ", msg->task_id, msg->cmd);
    } 
    else 
    {
//        ESP_LOGW(TAG, "[Sender %ld] queuse full transfer failed", msg->task_id);
    }
}

void send_scpspin_motor_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout)
{
	mt_message_t *msg = (mt_message_t *)message;
    msg->cmd = cmd;
    msg->angle = angle;
    msg->direction = dir;
    msg->timeout = timeout;
    BaseType_t status = xQueueSend(scpspin_motor_msg, msg, pdMS_TO_TICKS(100));
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
static void init_step_motor_gpio(void) {
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
}

static int step_motor_enable(step_motor_t mt, uint32_t enable)
{
	switch(mt)
	{
		case MAIN_COVER_MOTOR:
			gpio_set_level(MAIN_EN, enable);    
			break;
		case WASTE_COVER_MOTOR:
			gpio_set_level(WASTE_EN, enable);    
			break;
		case SCPSPIN_MOTOR:
			gpio_set_level(PIN_DRV_EN, enable);    
			gpio_set_level(PIN_DRV_NSLEEP, enable);    
			break;
		default:
			break;
	}
	return 0;
}

static void main_motor_stop(void) {
    // 1. 모터 구동 플래그 해제
    main_motor_run = false;
    
    // 2. 하드웨어 타이머 정지 (실행 중인 경우)
    if (main_motor_timer != NULL) {
        esp_timer_stop(main_motor_timer);
    }
    
    {
        gpio_set_level(MAIN_PWM_IN1, 0);
        gpio_set_level(MAIN_PWM_IN2, 0);
    }
		step_motor_enable(MAIN_COVER_MOTOR, 0);    
    // 4. 상태 변수 초기화
    main_motor_steps = 0;
}

static void main_motor_timer_callback(void* arg) 
{
    if (!main_motor_run) return;

    // 1. 현재 선택된 모터의 GPIO 핀 결정
    int gpio_in1 = MAIN_PWM_IN1;
    int gpio_in2 = MAIN_PWM_IN2;

    // 2. 현재 Phase 테이블 값 출력
    gpio_set_level(gpio_in1, phase_table[main_motor_phase][0]);
    gpio_set_level(gpio_in2, phase_table[main_motor_phase][1]);

    main_motor_steps--;
    if (main_motor_steps <= 0) {
        main_motor_run = false;
        esp_timer_stop(main_motor_timer);
        step_motor_enable(MAIN_COVER_MOTOR, 0);
	}
    if (main_motor_dir) {
        main_motor_phase = (main_motor_phase + 1) % 4; // 정방향 증가
    } else {
        main_motor_phase = (main_motor_phase - 1 + 4) % 4; // 역방향 감소
    }
}

static bool main_motor_move(int steps, int dir) 
{
    if (steps <= 0) return true;

    main_motor_steps = steps;
    main_motor_dir = dir;
    main_motor_phase = 0;
    main_motor_run = true;

    step_motor_enable(MAIN_COVER_MOTOR, 1);

    // 타이머가 없다면 최초 1회 생성
    if (main_motor_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = &main_motor_timer_callback,
            .name = "main_motor_pulse_timer"
        };
        esp_timer_create(&timer_args, &main_motor_timer);
    } else {
        // 기존에 작동 중이던 타이머 안전하게 정지
        esp_timer_stop(main_motor_timer);
    }

    // 타이머 시작 (MOTOR_SPEED_US = 2500)
    esp_timer_start_periodic(main_motor_timer, MOTOR_SPEED_US);
	return true;
}

static void waste_motor_stop(void) {
    // 1. 모터 구동 플래그 해제
    waste_motor_run = false;
    
    // 2. 하드웨어 타이머 정지 (실행 중인 경우)
    if (waste_motor_timer != NULL) {
        esp_timer_stop(waste_motor_timer);
    }
    
    {
        gpio_set_level(WASTE_PWM_IN1, 0);
        gpio_set_level(WASTE_PWM_IN2, 0);
    }
		step_motor_enable(WASTE_COVER_MOTOR, 0);    
    // 4. 상태 변수 초기화
    waste_motor_steps = 0;
}

static void waste_motor_timer_callback(void* arg) 
{
    if (!waste_motor_run) return;

    // 1. 현재 선택된 모터의 GPIO 핀 결정
    int gpio_in1 = WASTE_PWM_IN1;
    int gpio_in2 = WASTE_PWM_IN2;

    // 2. 현재 Phase 테이블 값 출력
    gpio_set_level(gpio_in1, phase_table[waste_motor_phase][0]);
    gpio_set_level(gpio_in2, phase_table[waste_motor_phase][1]);

    waste_motor_steps--;
    if (waste_motor_steps <= 0) {
        waste_motor_run = false;
        esp_timer_stop(waste_motor_timer);
        step_motor_enable(WASTE_COVER_MOTOR, 0);
	}
    if (waste_motor_dir) {
        waste_motor_phase = (waste_motor_phase + 1) % 4; // 정방향 증가
    } else {
        waste_motor_phase = (waste_motor_phase - 1 + 4) % 4; // 역방향 감소
    }
}

static bool waste_motor_move(int steps, int dir) 
{
    if (steps <= 0) return true;

    waste_motor_steps = steps;
    waste_motor_dir = dir;
    waste_motor_phase = 0;
    waste_motor_run = true;

    step_motor_enable(WASTE_COVER_MOTOR, 1);

    // 타이머가 없다면 최초 1회 생성
    if (waste_motor_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = &waste_motor_timer_callback,
            .name = "waste_motor_pulse_timer"
        };
        esp_timer_create(&timer_args, &waste_motor_timer);
    } else {
        // 기존에 작동 중이던 타이머 안전하게 정지
        esp_timer_stop(waste_motor_timer);
    }

    // 타이머 시작 (MOTOR_SPEED_US = 2500)
    esp_timer_start_periodic(waste_motor_timer, MOTOR_SPEED_US);
	return true;
}

static bool IRAM_ATTR mcpwm_timer_full_cb(mcpwm_timer_handle_t timer, const mcpwm_timer_event_data_t *edata, void *user_ctx) {
    if (scpspin_motor_run) {
        scpspin_cnt++; // 펄스가 피크(on_full)에 도달할 때마다 스텝 카운트 증가
        
        if (scpspin_cnt >= scpspin_steps) {
            // 목표 스텝(각도) 도달 시, 현재 주기를 마지막으로 하드웨어 타이머 정지 명령
            mcpwm_timer_start_stop(timer, MCPWM_TIMER_STOP_EMPTY);
            step_motor_enable(SCPSPIN_MOTOR, 0);
            scpspin_motor_run = false;
        }
    }
    // 기본적으로 다른 FreeRTOS 태스크로의 즉각적인 컨텍스트 스위칭이 필요 없으므로 false 반환
    return false; 
}

#define MCPWM_COMPARE_VALUE			1200//2000
static void init_mcpwm_step_generator(void) {
    ESP_LOGI(TAG, "%%s +", __func__);

    // 타이머 기본 설정: 1MHz 분해능, Up-Count 모드, 주기 2000us (500Hz)
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, 
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = MCPWM_COMPARE_VALUE,
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
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(mcpwm_cmpr, MCPWM_COMPARE_VALUE/2)); 

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

static void scpspin_stop(bool immediate) {
    ESP_LOGI(TAG, "Motor stop requested. Mode: %s", immediate ? "IMMEDIATE" : "SAFE");

    // 1. 이미 멈춰있는 상태라면 중복 처리 방지
    if (!scpspin_motor_run) {
        return;
    }

    // 2. 하드웨어 타이머 정지 명령 및 모터 드라이버 비활성화
    if (immediate) {
        // 즉시 정지: 타이머를 완전히 정지시키고 드라이버를 즉시 끕니다.
        mcpwm_timer_start_stop(mcpwm_timer, MCPWM_TIMER_STOP_EMPTY);
        step_motor_enable(SCPSPIN_MOTOR, 0);
    } else {
        // 안전 정지: 현재 주기가 끝날 때(EMPTY 상태가 될 때) 타이머가 멈추도록 예약합니다.
        mcpwm_timer_start_stop(mcpwm_timer, MCPWM_TIMER_STOP_EMPTY);
        
        // 타이머가 완전히 멈출 때까지 잠시 대기 (최대 현재 1주기 시간인 2ms 소요)
        // 안전 정지 시에는 드라이버를 바로 끄면 펄스가 잘릴 수 있으므로 타이머가 멈춘 후 끕니다.
        uint32_t timeout = 0;
        while (scpspin_motor_run && timeout < 10) { 
            vTaskDelay(pdMS_TO_TICKS(1));
            timeout++;
        }
        step_motor_enable(SCPSPIN_MOTOR, 0);
    }

    // 3. 소프트웨어 상태 변수 최종 정리 및 리셋
    scpspin_motor_run = false;
    scpspin_cnt = 0;
    scpspin_steps = 0;

    ESP_LOGI(TAG, "Motor cleanup finished successfully.");
}

static void scpspin_move(uint32_t steps, uint8_t direction) {
    gpio_set_level(PIN_DRV_DIR, direction);
    
    scpspin_cnt = 0;
    scpspin_steps = steps;
    scpspin_motor_run = true;

    step_motor_enable(SCPSPIN_MOTOR, 1);

    ESP_LOGI(TAG, "Motor Start: %s, Target Steps: %lu", (direction==1) ? "FORWARD" : "BACKWARD", steps);

    // 하드웨어 타이머 가동
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(mcpwm_timer, MCPWM_TIMER_START_NO_STOP));
#if 0   
    while (scpspin_motor_run) {
        float current_ma = read_current_ma(SCPSPIN_MOTOR);
        ESP_LOGI(TAG, "Driving... Current: %.1fmA, Progress: %lu/%lu", current_ma, scpspin_cnt, scpspin_steps);
        if (is_overcurrent_tripped(SCPSPIN_MOTOR)) {
            scpspin_stop(true); 	// immediate terminate
            ESP_LOGE(TAG, "Overcurrent Tripped! Motor Stopped Safety.");
            break;
        }
        
        // 너무 잦은 로그 방지 및 컨텍스트 스위칭 유도
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
    
    ESP_LOGI(TAG, "Motor Stopped Target Reached.");
#endif
}

static int set_main_motor_mode(int mode)
{
    if (xSemaphoreTake(main_mutex, portMAX_DELAY) == pdTRUE) 
    {
        main_step_mode = mode;
        xSemaphoreGive(main_mutex);
    }
    return 0;
}

static int set_waste_motor_mode(int mode)
{
    if (xSemaphoreTake(waste_mutex, portMAX_DELAY) == pdTRUE) 
    {
        waste_step_mode = mode;
        xSemaphoreGive(waste_mutex);
    }
    return 0;
}

static int set_scpspin_motor_mode(int mode)
{
    if (xSemaphoreTake(scpspin_mutex, portMAX_DELAY) == pdTRUE) 
    {
        scpspin_step_mode = mode;
        xSemaphoreGive(scpspin_mutex);
    }
    return 0;
}

bool get_stepmotor_run(step_motor_t mt)
{
	bool ret = false;
	if(mt == MAIN_COVER_MOTOR)
	{
		ret = main_motor_run;
	}
	else if(mt == WASTE_COVER_MOTOR)
	{
		ret = waste_motor_run;
	}
	else if(mt == SCPSPIN_MOTOR)
	{
		ret = scpspin_motor_run;
	}
	return ret;
}

int get_scpspin_cnt(void)
{
	return (int)scpspin_cnt;
}


static void main_monitor_task(void *arg)
{
    ESP_LOGI(TAG, "%s +", __func__);
    while (1)
    {
    	switch(main_step_mode)
    	{
			case MAIN_IDLE_MODE:
			break;
            case MAIN_WAIT_MODE:
                if(main_motor_run) 
                {
                }
                else
                {
                	ESP_LOGI(TAG, "main cover finished");
					set_main_motor_mode(MAIN_IDLE_MODE);
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

static void waste_monitor_task(void *arg)
{
    ESP_LOGI(TAG, "%s +", __func__);
    while (1)
    {
    	switch(waste_step_mode)
    	{
			case WASTE_IDLE_MODE:
			break;
            case WASTE_WAIT_MODE:
                if(waste_motor_run) 
                {
                }
                else
                {
                	ESP_LOGI(TAG, "waste cover finished");
					set_waste_motor_mode(WASTE_IDLE_MODE);
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

static void scpspin_monitor_task(void *arg)
{
    ESP_LOGI(TAG, "%s +", __func__);
    while (1)
    {
    	switch(scpspin_step_mode)
    	{
			case SCPSPIN_IDLE_MODE:
			break;
            case SCPSPIN_WAIT_MODE:
                if(scpspin_motor_run) 
                {
                }
                else
                {
                	ESP_LOGI(TAG, "scp spin finished");
					set_scpspin_motor_mode(SCPSPIN_IDLE_MODE);
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

void main_motor_cmd_task(void *arg) 
{
    ESP_LOGI(TAG, "%s +", __func__);
    while (1) {
        mt_message_t msg = {0};
        if (xQueueReceive(main_motor_msg, &msg, portMAX_DELAY) == pdPASS) {
//          ESP_LOGI(TAG, "[Receiver %ld] transfer complete -> cmd %d ", msg.task_id, msg.cmd);
            switch((int)(msg.cmd))
            {
                case MAIN_COVER_CMD:
	                if(msg.direction == FORWARD)
	                {
	                   	main_motor_move(MAIN_TARGET_STEPS, 1);
	                    set_main_motor_mode(MAIN_WAIT_MODE);
	               	}
	               	else if(msg.direction == REVERSE)
	               	{
	                   	main_motor_move(MAIN_TARGET_STEPS, 0);
	                    set_main_motor_mode(MAIN_WAIT_MODE);
	               	}
	               	else if(msg.direction == STOP)
	               	{
                        main_motor_stop();
                        set_main_motor_mode(MAIN_IDLE_MODE);
	               	}
                    break;
                
                default:
                break;
            }
        }
    }
//    ESP_LOGI(TAG, "%s -", __func__);
    vTaskDelete(NULL);
}

void waste_motor_cmd_task(void *arg) 
{
    ESP_LOGI(TAG, "%s +", __func__);
    while (1) {
        mt_message_t msg = {0};
        if (xQueueReceive(waste_motor_msg, &msg, portMAX_DELAY) == pdPASS) {
//          ESP_LOGI(TAG, "[Receiver %ld] transfer complete -> cmd %d ", msg.task_id, msg.cmd);
            switch((int)(msg.cmd))
            {
                case WASTE_COVER_CMD:
	                if(msg.direction == FORWARD)
	                {
	                   	waste_motor_move(WASTE_TARGET_STEPS, 1);
	                    set_waste_motor_mode(WASTE_WAIT_MODE);
	               	}
	               	else if(msg.direction == REVERSE)
	               	{
	                   	waste_motor_move(WASTE_TARGET_STEPS, 0);
	                    set_waste_motor_mode(WASTE_WAIT_MODE);
	               	}
	               	else if(msg.direction == STOP)
	               	{
                        waste_motor_stop();
                        set_waste_motor_mode(WASTE_IDLE_MODE);
	               	}
                    break;
                
                default:
                break;
            }
        }
    }
//    ESP_LOGI(TAG, "%s -", __func__);
    vTaskDelete(NULL);
}

void scpspin_motor_cmd_task(void *arg) 
{
    ESP_LOGI(TAG, "%s +", __func__);
    while (1) {
        mt_message_t msg = {0};
        if (xQueueReceive(scpspin_motor_msg, &msg, portMAX_DELAY) == pdPASS) {
//          ESP_LOGI(TAG, "[Receiver %ld] transfer complete -> cmd %d ", msg.task_id, msg.cmd);
            switch((int)(msg.cmd))
            {
                case SCP_SPIN_CMD:
                	if(msg.direction == FORWARD)
                	{
                        scpspin_move(DEG_TO_STEPS(msg.angle), 1);
                        set_scpspin_motor_mode(SCPSPIN_WAIT_MODE);
                	}
                	else if(msg.direction == REVERSE)
                	{
                        scpspin_move(DEG_TO_STEPS(msg.angle), 0);
                        set_scpspin_motor_mode(SCPSPIN_WAIT_MODE);
                	}
                	else if(msg.direction == STOP)
                	{
                        scpspin_stop(false); // safe terminate
                        set_scpspin_motor_mode(SCPSPIN_IDLE_MODE);
                	}
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
	
    main_mutex = xSemaphoreCreateMutex();
    waste_mutex = xSemaphoreCreateMutex();
    scpspin_mutex = xSemaphoreCreateMutex();

    init_step_motor_gpio();
    init_mcpwm_step_generator();	// scp spin

    main_motor_msg = xQueueCreate(10, sizeof(mt_message_t));
    waste_motor_msg = xQueueCreate(10, sizeof(mt_message_t));
    scpspin_motor_msg = xQueueCreate(10, sizeof(mt_message_t));

    xTaskCreate(main_monitor_task, "main_monitor_task", 4096, NULL, 5, NULL);
    xTaskCreate(waste_monitor_task, "waste_monitor_task", 4096, NULL, 5, NULL);
    xTaskCreate(scpspin_monitor_task, "scpspin_monitor_task", 4096, NULL, 5, NULL);

    xTaskCreatePinnedToCore(main_motor_cmd_task, "main_motor_cmd_task", 4096, NULL, 5, NULL, 1);
	xTaskCreatePinnedToCore(waste_motor_cmd_task, "waste_motor_cmd_task", 4096, NULL, 5, NULL, 1);
	xTaskCreatePinnedToCore(scpspin_motor_cmd_task, "scpspin_motor_cmd_task", 4096, NULL, 5, NULL, 1);
}
