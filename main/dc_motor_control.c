#include "main.h"

static const char *TAG = "DC_MOTOR_CTRL";

// 모터 방향 정의
#define MT_FORWARD_DIR  0
#define MT_REVERSE_DIR  1

#define PWM_FREQ_HZ             (25000)         // 25kHz PWM

///////////////////////////////////////////////////////////////////////
// SCP INOUT
#define SCP_INOUT_PWM           (GPIO_NUM_8)
#define SCP_INOUT_SEL           (GPIO_NUM_9)
static mcpwm_cmpr_handle_t comparator_scp = NULL;
volatile static bool mt_scp_inout_run = false;
volatile static bool mt_scp_inout_dir = false;
static QueueHandle_t scp_inout_motor_msg = NULL;


///////////////////////////////////////////////////////////////////////
// PLATE
#ifdef FEATURE_MAIN_COVER_DC_MOTOR	
#define PLATE_PWM               (GPIO_NUM_19)
#define PLATE_SEL               (GPIO_NUM_47)
#else
#define PLATE_PWM               (GPIO_NUM_45)
#define PLATE_SEL               (GPIO_NUM_38)
#endif

static mcpwm_cmpr_handle_t comparator_plate = NULL;
volatile static bool mt_plate_run = false;
volatile static bool mt_plate_dir = false;
static QueueHandle_t plate_motor_msg = NULL;

void send_plate_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout)
{
	mt_message_t *msg = (mt_message_t *)message;
    msg->cmd = cmd;
    msg->angle = angle;
    msg->direction = dir;
    msg->timeout = timeout;
    BaseType_t status = xQueueSend(plate_motor_msg, msg, pdMS_TO_TICKS(100));
    if (status == pdPASS) 
    {
//        ESP_LOGI(TAG, "[Sender %ld] transfer complete -> cmd %d ", msg->task_id, msg->cmd);
    } 
    else 
    {
//        ESP_LOGW(TAG, "[Sender %ld] queuse full transfer failed", msg->task_id);
    }
}

void send_scpinout_msg(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t timeout)
{
	mt_message_t *msg = (mt_message_t *)message;
    msg->cmd = cmd;
    msg->angle = angle;
    msg->direction = dir;
    msg->timeout = timeout;
    BaseType_t status = xQueueSend(scp_inout_motor_msg, msg, pdMS_TO_TICKS(100));
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

static void scp_inout_motor_move(int speed_pct)
{
    ESP_LOGI(TAG, "%s speed_pct %d ", __func__, speed_pct);
/*    if (scp_inout_fault) {
        mcpwm_comparator_set_compare_value(comparator_scp, 0);
        return;
    } */
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

static void plate_motor_move(int speed_pct)
{
//    ESP_LOGI(TAG, "%s speed_pct %d ", __func__, speed_pct);
/*    if (plate_fault) {
        mcpwm_comparator_set_compare_value(comparator_plate, 0);
        return;
    } */
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

bool get_dcmotor_run(dc_motor_t mt)
{
	bool ret = false;
	if(mt == PLATE_MOTOR)
	{
		ret = mt_plate_run;
	}
	else if(mt == SCPINOUT_MOTOR)
	{
		ret = mt_scp_inout_run;
	}
	return ret;
}

bool get_dcmotor_dir(dc_motor_t mt)
{
	bool ret = false;
	if(mt == PLATE_MOTOR)
	{
		ret = mt_plate_dir;
	}
	else if(mt == SCPINOUT_MOTOR)
	{
		ret = mt_scp_inout_dir;
	}
	return ret;
}

void plate_cmd_task(void *arg) {
    ESP_LOGI(TAG, "%s +", __func__);

    while (1) {
        mt_message_t msg = {0};
        if (xQueueReceive(plate_motor_msg, &msg, portMAX_DELAY) == pdPASS) {
//        	ESP_LOGI(TAG, "[Receiver %ld] transfer complete -> cmd %d ", msg.task_id, msg.cmd);
            switch((int)(msg.cmd))
            {
                case PLATE_CMD:
                	if(msg.direction == FORWARD)
                	{
//                		ESP_LOGI(TAG, "%s forward ", __func__);
						plate_motor_move(-99);
                        mt_plate_dir = false;
                        mt_plate_run = true;
                	}
                	else if(msg.direction == REVERSE)
                	{
//                		ESP_LOGI(TAG, "%s reverse ", __func__);
						plate_motor_move(99);
                        mt_plate_dir = true;
                        mt_plate_run = true;
                	}
                	else if(msg.direction == STOP)
                	{
//                		ESP_LOGI(TAG, "%s stop ", __func__);
                        plate_motor_move(0);
                        mt_plate_run = false;
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

void scpinout_cmd_task(void *arg) {
    ESP_LOGI(TAG, "%s +", __func__);

    while (1) {
        mt_message_t msg = {0};
        if (xQueueReceive(scp_inout_motor_msg, &msg, portMAX_DELAY) == pdPASS) {
//        	ESP_LOGI(TAG, "[Receiver %ld] transfer complete -> cmd %d ", msg.task_id, msg.cmd);
            switch((int)(msg.cmd))
            {
                case SCP_INOUT_CMD:
                	if(msg.direction == FORWARD)
                	{
                		ESP_LOGI(TAG, "debug 1 ");
						scp_inout_motor_move(-99);
                        mt_scp_inout_dir = false;
                        mt_scp_inout_run = true;
                		ESP_LOGI(TAG, "debug 2 ");
                	}
                	else if(msg.direction == REVERSE)
                	{
						scp_inout_motor_move(99);
                        mt_scp_inout_dir = true;
                        mt_scp_inout_run = true;
                	}
                	else if(msg.direction == STOP)
                	{
                        scp_inout_motor_move(0);
                        mt_scp_inout_run = false;
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

void dc_motor_init(void)
{
    ESP_LOGI(TAG, "%s", __func__);
	
    init_system_gpios();
    init_mcpwm_system();

    plate_motor_msg = xQueueCreate(10, sizeof(mt_message_t));
    scp_inout_motor_msg = xQueueCreate(10, sizeof(mt_message_t));

    // 2. 모니터링 태스크 생성
    xTaskCreate(plate_cmd_task, "plate_cmd_task", 3072, NULL, 5, NULL);
    xTaskCreate(scpinout_cmd_task, "scpinout_cmd_task", 3072, NULL, 5, NULL);
}
