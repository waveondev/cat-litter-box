#include "main.h"

static const char *TAG = "STEP_MOTOR_CTRL";

///////////////////////////////////////////////////////////////////////
// WASTE COVER
#define WASTE_PWM_IN1       GPIO_NUM_13
#define WASTE_PWM_IN2       GPIO_NUM_11
#define WASTE_EN         	GPIO_NUM_12             

#define WASTE_TARGET_STEPS (1020+100)
#define MOTOR_SPEED_MS     12
#define MOTOR_SPEED_US     (MOTOR_SPEED_MS*1000)

///////////////////////////////////////////////////////////////////////
// scoop spin 
#define PIN_DRV_STEP          GPIO_NUM_6
#define PIN_DRV_DIR           GPIO_NUM_20
#define PIN_DRV_EN            GPIO_NUM_5
#define PIN_DRV_NSLEEP        GPIO_NUM_46

#define MOTOR_STEP_ANGLE      1.8f          	
#define MICROSTEPPING         51

#define DEG_TO_STEPS(deg)  ((uint32_t)(((float)(deg) / MOTOR_STEP_ANGLE) * MICROSTEPPING))
static mcpwm_timer_handle_t   mcpwm_timer = NULL;
static mcpwm_cmpr_handle_t    mcpwm_cmpr = NULL;

volatile uint32_t scpspin_cnt = 0;
volatile uint32_t scpspin_steps = 0;
static bool scpspin_motor_dir = false;
volatile bool scpspin_motor_run = false;

static int waste_motor_steps = 0;
static int waste_motor_phase = 0;
static bool waste_motor_dir = false;
volatile static bool waste_motor_run = false;
static esp_timer_handle_t waste_motor_timer = NULL;

static const int phase_table[4][2] = {
    {1, 0}, 
    {1, 1}, 
    {0, 1}, 
    {0, 0}
};

static QueueHandle_t waste_motor_msg = NULL;
static QueueHandle_t scpspin_motor_msg = NULL;

static uint32_t new_spin_period = 0;
static uint32_t new_spin_tick = 0;

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
//        ESP_LOGW(TAG, "[Sender %ld] queue full transfer failed", msg->task_id);
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
//        ESP_LOGW(TAG, "[Sender %ld] queue full transfer failed", msg->task_id);
    }
}

void send_scpspin_motor_msg_ex(void *message, uint32_t cmd, uint32_t angle, uint32_t dir, uint32_t speed, uint32_t timeout, bool cal)
{
    mt_message_t *msg = (mt_message_t *)message;
    msg->cmd = cmd;
    msg->angle = angle;
    msg->direction = dir;
    msg->speed = speed;     // mt_message_t에 speed가 없다면 timeout 등 별도 필드로 수신부와 약속 후 전달 가능
    msg->timeout = timeout;
    msg->cal = cal;
    
    BaseType_t status = xQueueSend(scpspin_motor_msg, msg, pdMS_TO_TICKS(100));
    if (status != pdPASS) {
        // ESP_LOGW(TAG, "[Sender %ld] queue full transfer failed", msg->task_id);
    }
}

static void init_step_motor_gpio(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << WASTE_PWM_IN1) | (1ULL << WASTE_PWM_IN2) | (1ULL << WASTE_EN) |
                        (1ULL << PIN_DRV_STEP) | (1ULL << PIN_DRV_DIR) | (1ULL << PIN_DRV_EN) | (1ULL << PIN_DRV_NSLEEP),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);

    gpio_set_level(WASTE_EN, 0);    
    gpio_set_level(PIN_DRV_EN, 0);
    gpio_set_level(PIN_DRV_NSLEEP, 0);
    gpio_set_level(PIN_DRV_DIR, 1);

    gpio_reset_pin(WASTE_PWM_IN1);
    gpio_reset_pin(WASTE_PWM_IN2);
    gpio_set_direction(WASTE_PWM_IN1, GPIO_MODE_OUTPUT);
    gpio_set_direction(WASTE_PWM_IN2, GPIO_MODE_OUTPUT);
    gpio_set_level(WASTE_PWM_IN1, 0);
    gpio_set_level(WASTE_PWM_IN2, 0);

}

static int step_motor_enable(step_motor_t mt, uint32_t enable)
{
	switch(mt)
	{
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

static void waste_motor_stop(void) {
    ESP_LOGI(TAG, "%s +", __func__);

    waste_motor_run = false;
    
    if (waste_motor_timer != NULL) {
        esp_timer_stop(waste_motor_timer);
    }
    
    {
        gpio_set_level(WASTE_PWM_IN1, 0);
        gpio_set_level(WASTE_PWM_IN2, 0);
    }
		step_motor_enable(WASTE_COVER_MOTOR, 0);    
    waste_motor_steps = 0;
}

static void waste_motor_timer_callback(void* arg) 
{
    if (!waste_motor_run) return;

    int gpio_in1 = WASTE_PWM_IN1;
    int gpio_in2 = WASTE_PWM_IN2;

    gpio_set_level(gpio_in1, phase_table[waste_motor_phase][0]);
    gpio_set_level(gpio_in2, phase_table[waste_motor_phase][1]);

    waste_motor_steps--;
    if (waste_motor_steps <= 0) {
        waste_motor_run = false;
        esp_timer_stop(waste_motor_timer);
        step_motor_enable(WASTE_COVER_MOTOR, 0);
	}
    if (waste_motor_dir) {
        waste_motor_phase = (waste_motor_phase + 1) % 4;
    } else {
        waste_motor_phase = (waste_motor_phase - 1 + 4) % 4;
    }
}

static bool waste_motor_move(int steps, bool dir) 
{
    ESP_LOGI(TAG, "%s +", __func__);

    if (steps <= 0) return true;

    waste_motor_steps = steps;
    waste_motor_dir = dir;
    waste_motor_phase = 0;
    waste_motor_run = true;

    step_motor_enable(WASTE_COVER_MOTOR, 1);

    if (waste_motor_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = &waste_motor_timer_callback,
            .name = "waste_motor_pulse_timer"
        };
        esp_timer_create(&timer_args, &waste_motor_timer);
    } else {
        esp_timer_stop(waste_motor_timer);
    }

    esp_timer_start_periodic(waste_motor_timer, MOTOR_SPEED_US);
	return true;
}

int get_waste_step_cnt(void)
{
	return (int)waste_motor_steps;
}

static bool IRAM_ATTR mcpwm_timer_full_cb(mcpwm_timer_handle_t timer, const mcpwm_timer_event_data_t *edata, void *user_ctx) {
    if (scpspin_motor_run) {
        scpspin_cnt++; 
        
        if (scpspin_cnt >= scpspin_steps) {
            mcpwm_timer_start_stop(timer, MCPWM_TIMER_STOP_EMPTY);
            if(!check_spin_mt_on())
            	step_motor_enable(SCPSPIN_MOTOR, 0);
            scpspin_motor_run = false;
        }
    }
    return false; 
}

#define MCPWM_COMPARE_VALUE			2000
static void init_mcpwm_step_generator(void) {
    ESP_LOGI(TAG, "%s %d+", __func__, MCPWM_COMPARE_VALUE);

    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, 
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = MCPWM_COMPARE_VALUE,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &mcpwm_timer));

    new_spin_period = (uint32_t)MCPWM_COMPARE_VALUE;

    mcpwm_operator_config_t operator_config = { .group_id = 0 };
    mcpwm_oper_handle_t oper = NULL;
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, mcpwm_timer));

    mcpwm_comparator_config_t comparator_config = { .flags.update_cmp_on_tez = true };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &mcpwm_cmpr));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(mcpwm_cmpr, MCPWM_COMPARE_VALUE/2)); 

    mcpwm_generator_config_t generator_config = { .gen_gpio_num = PIN_DRV_STEP };
    mcpwm_gen_handle_t generator = NULL;
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));

    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_timer_event(generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH),
        MCPWM_GEN_TIMER_EVENT_ACTION_END()));
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, mcpwm_cmpr, MCPWM_GEN_ACTION_LOW),
        MCPWM_GEN_COMPARE_EVENT_ACTION_END()));

    mcpwm_timer_event_callbacks_t cbs = {
        .on_full  = mcpwm_timer_full_cb,
        .on_empty = NULL,
        .on_stop  = NULL
    };
    
    ESP_ERROR_CHECK(mcpwm_timer_register_event_callbacks(mcpwm_timer, &cbs, NULL));
    
    ESP_ERROR_CHECK(mcpwm_timer_enable(mcpwm_timer));
}

static void scpspin_stop(bool immediate) {
    ESP_LOGI(TAG, "Motor stop requested. Mode: %s", immediate ? "IMMEDIATE" : "SAFE");

    if (!scpspin_motor_run) {
        return;
    }

    if (immediate) {
        mcpwm_timer_start_stop(mcpwm_timer, MCPWM_TIMER_STOP_EMPTY);
        if(!check_spin_mt_on())
        	step_motor_enable(SCPSPIN_MOTOR, 0);
    } else {
        mcpwm_timer_start_stop(mcpwm_timer, MCPWM_TIMER_STOP_EMPTY);
        uint32_t timeout = 0;
        while (scpspin_motor_run && timeout < 10) { 
            vTaskDelay(pdMS_TO_TICKS(1));
            timeout++;
        }
        if(!check_spin_mt_on())
        	step_motor_enable(SCPSPIN_MOTOR, 0);
    }

    scpspin_motor_run = false;
    scpspin_cnt = 0;
    scpspin_steps = 0;

    ESP_LOGI(TAG, "Motor cleanup finished successfully.");
}

#define SCP_SPIN_SPEED_STEP				(100)
static void scpspin_speed_dir(int dir)
{

	ESP_LOGI(TAG, "%s dir %d spin_period %d +", __func__, dir, (int)new_spin_period);

    if (mcpwm_timer == NULL || mcpwm_cmpr == NULL) return;

	if(dir)
	{
        new_spin_period += SCP_SPIN_SPEED_STEP;
        new_spin_tick = new_spin_period /2;
	}
	else
	{
		new_spin_period -= SCP_SPIN_SPEED_STEP;
		new_spin_tick  = new_spin_period /2;
	}
    uint32_t compare_ticks = new_spin_period / 2;
    ESP_ERROR_CHECK(mcpwm_timer_set_period(mcpwm_timer, new_spin_period));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(mcpwm_cmpr, compare_ticks));
	ESP_LOGI(TAG, "%s spin_period %d changed ", __func__, (int)new_spin_period);
}

static void scpspin_speed(uint32_t period)
{

	ESP_LOGI(TAG, "%s spin_period %d +", __func__, (int)new_spin_period);

    if (mcpwm_timer == NULL || mcpwm_cmpr == NULL) return;

    new_spin_period = period;
    new_spin_tick = new_spin_period /2;
    uint32_t compare_ticks = new_spin_period / 2;
    ESP_ERROR_CHECK(mcpwm_timer_set_period(mcpwm_timer, new_spin_period));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(mcpwm_cmpr, compare_ticks));
	ESP_LOGI(TAG, "%s spin_period %d changed ", __func__, (int)new_spin_period);
}


static void scpspin_move(uint32_t steps, bool direction) {
    gpio_set_level(PIN_DRV_DIR, direction);
    
    scpspin_cnt = 0;
    scpspin_steps = steps;
    scpspin_motor_run = true;
	scpspin_motor_dir = direction;
	
    step_motor_enable(SCPSPIN_MOTOR, 1);

    ESP_LOGI(TAG, "Motor Start: %s, Target Steps: %lu", (direction==1) ? "FORWARD" : "BACKWARD", steps);
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(mcpwm_timer, MCPWM_TIMER_START_NO_STOP));
}

bool get_stepmotor_run(step_motor_t mt)
{
	bool ret = false;
	if(mt == WASTE_COVER_MOTOR)
	{
		ret = waste_motor_run;
	}
	else if(mt == SCPSPIN_MOTOR)
	{
		ret = scpspin_motor_run;
	}
	return ret;
}

bool get_stepmotor_dir(step_motor_t mt)
{
	bool ret = false;
	if(mt == WASTE_COVER_MOTOR)
	{
		ret = waste_motor_dir;
	}
	else if(mt == SCPSPIN_MOTOR)
	{
		ret = scpspin_motor_dir;
	}
	return ret;
}


int get_scpspin_cnt(void)
{
	return (int)scpspin_cnt;
}

int get_scpspin_remain_cnt(void)
{
	return (int)(scpspin_steps - scpspin_cnt);
}

static void waste_motor_cmd_task(void *arg) 
{
    ESP_LOGI(TAG, "%s +", __func__);
    while (1) {
        mt_message_t msg = {0};
        
        if (xQueueReceive(waste_motor_msg, &msg, pdMS_TO_TICKS(10)) == pdPASS) {
            switch((int)(msg.cmd))
            {
                case WASTE_COVER_CMD:
                    if(msg.direction == FORWARD)
                    {
                        waste_motor_move(WASTE_TARGET_STEPS, true);
                    }
                    else if(msg.direction == REVERSE)
                    {
                        waste_motor_move(WASTE_TARGET_STEPS, false);
                    }
                    else if(msg.direction == STOP)
                    {
                        waste_motor_stop();
                    }
                    break;
                case WASTE_STEP_CMD:
                    if(msg.direction == FORWARD)
                    {
                        waste_motor_move(msg.angle, true);
                    }
                    else if(msg.direction == REVERSE)
                    {
                        waste_motor_move(msg.angle, false);
                    }
                    else if(msg.direction == STOP)
                    {
                        waste_motor_stop();
                    }                
                    break;
                default:
                break;
            }
        }

        if (waste_motor_run) {
            int pt_status = pt_check(STEP_MOTOR, WASTE_COVER_MOTOR);
            
            if (waste_motor_dir == true && pt_status == 1) {
                ESP_LOGI(TAG, "WASTE_COVER OPEN sensor detected. Auto stop.");
                waste_motor_stop();
            }
            else if (waste_motor_dir == false && pt_status == -1) {
                ESP_LOGI(TAG, "WASTE_COVER CLOSE sensor detected. Auto stop.");
                waste_motor_stop();
            }
        }
    }
    vTaskDelete(NULL);
}



static void scpspin_motor_cmd_task(void *arg) 
{
    ESP_LOGI(TAG, "%s +", __func__);
    while (1) {
        mt_message_t msg = {0};
        
        if (xQueueReceive(scpspin_motor_msg, &msg, pdMS_TO_TICKS(10)) == pdPASS) {
            switch((int)(msg.cmd))
            {
                case SCP_SPIN_CMD:
                    if(msg.direction == FORWARD)
                    {
                        scpspin_move(DEG_TO_STEPS(msg.angle), true);
                    }
                    else if(msg.direction == REVERSE)
                    {
                        scpspin_move(DEG_TO_STEPS(msg.angle), false);
                    }
                    else if(msg.direction == STOP)
                    {
                        scpspin_stop(false); // safe terminate
                    }
                    break;

                case SCP_SPIN_STEP_CMD:
                    if(msg.direction == FORWARD)
                    {
                        scpspin_move(msg.angle, true);
                    }
                    else if(msg.direction == REVERSE)
                    {
                        scpspin_move(msg.angle, false);
                    }
                    break;
                case SCP_SPIN_SPEED_CMD:
                    if(msg.angle != 0)
                    {
                        scpspin_speed(msg.angle);
                    }
                    else
                    {
                        if(msg.direction == FORWARD)
                        {
                            scpspin_speed_dir(1);
                        }
                        else
                        {
                            scpspin_speed_dir(0);
                        }
                    }
                    break;
                default:
                    break;
            }
        }

        if (scpspin_motor_run) {
            int pt_status = pt_check(STEP_MOTOR, SCPSPIN_MOTOR);
            
            if (scpspin_motor_dir == true && pt_status == 1) {
                ESP_LOGI(TAG, "SCP_SPIN FORWARD sensor detected. Auto stop.");
                scpspin_stop(true);
            }
            else if (scpspin_motor_dir == false && pt_status == -1) {
                ESP_LOGI(TAG, "SCP_SPIN REVERSE sensor detected. Auto stop.");
                scpspin_stop(true);
            }
        }
    }
    vTaskDelete(NULL);
}

void step_motor_init(void) 
{
	ESP_LOGI(TAG, "%s", __func__);

    init_step_motor_gpio();
    init_mcpwm_step_generator();	// scp spin

    waste_motor_msg = xQueueCreate(10, sizeof(mt_message_t));
    scpspin_motor_msg = xQueueCreate(10, sizeof(mt_message_t));

	xTaskCreatePinnedToCore(waste_motor_cmd_task, "waste_motor_cmd_task", 4096, NULL, 5, NULL, 1);
	xTaskCreatePinnedToCore(scpspin_motor_cmd_task, "scpspin_motor_cmd_task", 8192, NULL, 5, NULL, 1);
}
