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
#define PLATE_PWM               (GPIO_NUM_45)
#define PLATE_SEL               (GPIO_NUM_38)

// MAIN COVER
#define MAIN_PWM        		(GPIO_NUM_42)// (GPIO_NUM_19)	// 42
#define MAIN_SEL        		(GPIO_NUM_39)// (GPIO_NUM_47)	// 39
//#define MAIN_EN         	GPIO_NUM_21             

static mcpwm_cmpr_handle_t comparator_plate = NULL;
static mcpwm_cmpr_handle_t comparator_main = NULL;
volatile static bool mt_plate_run = false;
volatile static bool mt_plate_dir = false;
volatile static bool mt_main_run = false;
volatile static bool mt_main_dir = false;
static QueueHandle_t plate_motor_msg = NULL;
static QueueHandle_t main_motor_msg = NULL;

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
//        ESP_LOGW(TAG, "[Sender %ld] queue full transfer failed", msg->task_id);
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
//        ESP_LOGW(TAG, "[Sender %ld] queue full transfer failed", msg->task_id);
    }
}

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
//        ESP_LOGW(TAG, "[Sender %ld] queue full transfer failed", msg->task_id);
    }
}

static void init_system_gpios(void)
{
    ESP_LOGI(TAG, "%s", __func__);
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SCP_INOUT_SEL) | (1ULL << PLATE_SEL)| (1ULL << MAIN_SEL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    gpio_set_level(SCP_INOUT_SEL, MT_FORWARD_DIR);
    gpio_set_level(PLATE_SEL, MT_FORWARD_DIR);
    gpio_set_level(MAIN_SEL, MT_FORWARD_DIR);
}

static void init_mcpwm_system(void)
{
    ESP_LOGI(TAG, "%s", __func__);

    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000, 
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = 10000000 / PWM_FREQ_HZ, // 400 Ticks
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    mcpwm_operator_config_t oper_config = { .group_id = 0 };
    mcpwm_comparator_config_t cmpr_config = { .flags.update_cmp_on_tez = true };

    mcpwm_oper_handle_t oper_scp = NULL;
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_config, &oper_scp));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper_scp, timer));

    mcpwm_gen_handle_t gen_scp = NULL;
    mcpwm_generator_config_t gen_cfg_scp = { .gen_gpio_num = SCP_INOUT_PWM };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper_scp, &gen_cfg_scp, &gen_scp));
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper_scp, &cmpr_config, &comparator_scp));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen_scp, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_scp, MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator_scp, MCPWM_GEN_ACTION_LOW)));

    mcpwm_oper_handle_t oper_plate = NULL;
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_config, &oper_plate));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper_plate, timer));

    mcpwm_gen_handle_t gen_plate = NULL;
    mcpwm_generator_config_t gen_cfg_plate = { .gen_gpio_num = PLATE_PWM };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper_plate, &gen_cfg_plate, &gen_plate));
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper_plate, &cmpr_config, &comparator_plate));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen_plate, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_plate, MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator_plate, MCPWM_GEN_ACTION_LOW)));

    mcpwm_timer_handle_t timer2 = NULL;
    mcpwm_timer_config_t timer_config2 = {
        .group_id = 1,  // <--- 0에서 1로 변경
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000, 
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = 10000000 / PWM_FREQ_HZ,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config2, &timer2));
    mcpwm_operator_config_t oper_config2 = { .group_id = 1 }; // <--- 0에서 1로 변경
    
    mcpwm_oper_handle_t oper_main = NULL;
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_config2, &oper_main));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper_main, timer2));

    mcpwm_gen_handle_t gen_main = NULL;
    mcpwm_generator_config_t gen_cfg_main = { .gen_gpio_num = MAIN_PWM };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper_main, &gen_cfg_main, &gen_main));
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper_main, &cmpr_config, &comparator_main));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen_main, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_main, MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator_main, MCPWM_GEN_ACTION_LOW)));

    // 5. 초기 듀티비 0% 설정 및 타이머 활성화/시작
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator_scp, 0));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator_plate, 0));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator_main, 0));

    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer2));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer2, MCPWM_TIMER_START_NO_STOP));
    
    ESP_LOGI(TAG, "%s finished", __func__);
}

// Scoop In/Out
static void scp_inout_motor_move(int speed_pct)
{
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

// Plate : DC motor
static void plate_motor_move(int speed_pct)
{
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

// Main Cover : DC motor
static void main_motor_move(int speed_pct)
{
    uint32_t compare_val = (uint32_t)((abs(speed_pct) * 400) / 100);

    if (speed_pct > 0) {
        mcpwm_comparator_set_compare_value(comparator_main, compare_val);
        gpio_set_level(MAIN_SEL, MT_FORWARD_DIR);
    } else if (speed_pct < 0) {
        mcpwm_comparator_set_compare_value(comparator_main, compare_val);
        gpio_set_level(MAIN_SEL, MT_REVERSE_DIR);
    } else {
        mcpwm_comparator_set_compare_value(comparator_main, 0);
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
	else if(mt == MAIN_COVER_MOTOR)
	{
		ret = mt_main_run;
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
	else if(mt == MAIN_COVER_MOTOR)
	{
		ret = mt_main_dir;
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
						plate_motor_move(-99);
                        mt_plate_dir = true;
                        mt_plate_run = true;
                	}
                	else if(msg.direction == REVERSE)
                	{
						plate_motor_move(99);
                        mt_plate_dir = false;
                        mt_plate_run = true;
                	}
                	else if(msg.direction == STOP)
                	{
                        plate_motor_move(0);
                        mt_plate_run = false;
                	}
                    break;
                
                default:
                break;
            }
        }
    }
    vTaskDelete(NULL);
}

void scpinout_cmd_task(void *arg) {
    ESP_LOGI(TAG, "%s +", __func__);

    while (1) {
        mt_message_t msg = {0};
        
        if (xQueueReceive(scp_inout_motor_msg, &msg, pdMS_TO_TICKS(10)) == pdPASS) {
            switch((int)(msg.cmd))
            {
                case SCP_INOUT_CMD:
                    if(msg.direction == FORWARD)
                    {
                        scp_inout_motor_move(-99);
                        mt_scp_inout_dir = true;     // true: FORWARD (OUT 방향)
                        mt_scp_inout_run = true;
                    }
                    else if(msg.direction == REVERSE)
                    {
                        scp_inout_motor_move(99);
                        mt_scp_inout_dir = false;    // false: REVERSE (IN 방향)
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

        // 수정(복원)된 부분: 구동 방향에 일치하는 센서가 감지되었을 때만 정지
        if (mt_scp_inout_run) {
            int pt_status = pt_check(DC_MOTOR, SCPINOUT_MOTOR);
            
            // FORWARD (스쿠퍼 OUT) 구동 중 OUT 위치 센서(1) 감지 시
            if (mt_scp_inout_dir == true && pt_status == 1) {
                ESP_LOGI(TAG, "SCP_OUT sensor detected. Auto stop.");
                scp_inout_motor_move(0);
                mt_scp_inout_run = false;
            }
            // REVERSE (스쿠퍼 IN) 구동 중 IN 위치 센서(-1) 감지 시
            else if (mt_scp_inout_dir == false && pt_status == -1) {
                ESP_LOGI(TAG, "SCP_IN sensor detected. Auto stop.");
                scp_inout_motor_move(0);
                mt_scp_inout_run = false;
            }
        }
    }
    vTaskDelete(NULL);
}


void main_cmd_task(void *arg) {
    ESP_LOGI(TAG, "%s +", __func__);

    while (1) {
        mt_message_t msg = {0};
        
        if (xQueueReceive(main_motor_msg, &msg, pdMS_TO_TICKS(10)) == pdPASS) {
            switch((int)(msg.cmd))
            {
                case MAIN_COVER_CMD:
                    if(msg.direction == FORWARD)
                    {
                        main_motor_move(-60);
                        mt_main_dir = true;     // true: FORWARD (OPEN 방향)
                        mt_main_run = true;
                    }
                    else if(msg.direction == REVERSE)
                    {
                        main_motor_move(60);
                        mt_main_dir = false;    // false: REVERSE (CLOSE 방향)
                        mt_main_run = true;
                    }
                    else if(msg.direction == STOP)
                    {
                        main_motor_move(0);
                        mt_main_run = false;
                    }
                    break;
                
                default:
                break;
            }
        }

        // 수정(복원)된 부분: 구동 방향에 일치하는 센서가 감지되었을 때만 정지
        if (mt_main_run) {
            int pt_status = pt_check(DC_MOTOR, MAIN_COVER_MOTOR);
            
            // FORWARD (열림) 구동 중 열림 센서(1) 감지 시
            if (mt_main_dir == true && pt_status == 1) {
                ESP_LOGI(TAG, "MCOVER OPEN sensor detected. Auto stop.");
                main_motor_move(0);
                mt_main_run = false;
            }
            // REVERSE (닫힘) 구동 중 닫힘 센서(-1) 감지 시
            else if (mt_main_dir == false && pt_status == -1) {
                ESP_LOGI(TAG, "MCOVER CLOSE sensor detected. Auto stop.");
                main_motor_move(0);
                mt_main_run = false;
            }
        }
    }
    vTaskDelete(NULL);
}


void dc_motor_init(void)
{
    ESP_LOGI(TAG, "%s", __func__);
	
    init_system_gpios();
    init_mcpwm_system();

    plate_motor_msg = xQueueCreate(10, sizeof(mt_message_t));
    scp_inout_motor_msg = xQueueCreate(10, sizeof(mt_message_t));
    main_motor_msg = xQueueCreate(10, sizeof(mt_message_t));

    xTaskCreate(plate_cmd_task, "plate_cmd_task", 3072, NULL, 5, NULL);
    xTaskCreate(scpinout_cmd_task, "scpinout_cmd_task", 3072, NULL, 5, NULL);
    xTaskCreate(main_cmd_task, "main_cmd_task", 4096, NULL, 5, NULL);
}
