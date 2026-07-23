#include "main.h"

static const char *TAG = "CURRENT_MON";

#define ADC_MAX_CH				5

#define SCP_INOUT_ADC_GPIO     	GPIO_NUM_3
#define PLATE_ADC_GPIO  		GPIO_NUM_2
#define MAIN_ADC_GPIO     		GPIO_NUM_4
#define WASTE_ADC_GPIO     		GPIO_NUM_10
#define SCP_SPIN_ADC_GPIO       GPIO_NUM_7

#define SCP_INOUT_ADC_CH        ADC_CHANNEL_2
#define PLATE_ADC_CH            ADC_CHANNEL_1
#define MAIN_ADC_CH     		ADC_CHANNEL_3
#define WASTE_ADC_CH     		ADC_CHANNEL_9
#define SCP_SPIN_ADC_CH       	ADC_CHANNEL_6

static adc_continuous_handle_t 	adc_handle = NULL;

static bool scp_inout_fault = false;
static bool plate_fault = false;
static bool main_fault = false;
static bool waste_fault = false;
static bool scp_spin_fault = false;

#define OVERCURRENT_THRESHOLD_MA 	1200
#define INA180A1_GAIN        		20
#define ADC_VREF_MV           		3300
#define SHUNT_MILI_OHM    			100	// 100 mili-Ohm
#define ADC_MAX_VAL           		4095

#define ADC_OUTPUT_BIT_WIDTH    	(SOC_ADC_DIGI_MAX_BITWIDTH) // 12-bit
#define ADC_READ_LEN            	1024//(2000)

#if 0
//#define MAX_CURRENT_LIMIT  		0.5f           // 35BYJ46 소형 모터 보호를 위해 0.5A로 하향 조정
#define MAX_CURRENT_LIMIT  			1.20f
#define SHUNT_RESISTOR_OHM    	0.1f          // 션트 저항값 (100mOhm)
#define INA180A1_GAIN        	20.0f          
#define ADC_VREF_MV           	3300.0f       // ADC 기준 전압 (3.3V)
#define ADC_MAX_VAL           	4095.0f       // 12비트 ADC 최대값
#endif

/**
 * 4. ADC 연속 모드 초기화 (두 채널을 멀티플렉싱 스캔하도록 통합)
 */
static void init_adc_continuous_system(void)
{
    ESP_LOGI(TAG, "%s", __func__);

	adc_continuous_handle_cfg_t adc_config = {
	 .max_store_buf_size = 6000,
	 .conv_frame_size = ADC_READ_LEN, 
	};
	ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

	// 2. 글로벌 디지털 컨트롤러 설정
	adc_continuous_config_t dig_config = {
	 .sample_freq_hz = 50 * 1000,
	 .conv_mode = ADC_CONV_SINGLE_UNIT_1,
	 .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
	};

	// 3. 다중 채널 스캔을 위한 패턴 구성
	static adc_digi_pattern_config_t adc_patterns[ADC_MAX_CH] = {
	 { .atten = ADC_ATTEN_DB_12, .channel = SCP_INOUT_ADC_CH, .unit = ADC_UNIT_1, .bit_width = ADC_OUTPUT_BIT_WIDTH },
	 { .atten = ADC_ATTEN_DB_12, .channel = PLATE_ADC_CH,     .unit = ADC_UNIT_1, .bit_width = ADC_OUTPUT_BIT_WIDTH },
	 { .atten = ADC_ATTEN_DB_12, .channel = MAIN_ADC_CH,      .unit = ADC_UNIT_1, .bit_width = ADC_OUTPUT_BIT_WIDTH },
	 { .atten = ADC_ATTEN_DB_12, .channel = WASTE_ADC_CH,     .unit = ADC_UNIT_1, .bit_width = ADC_OUTPUT_BIT_WIDTH },
	 { .atten = ADC_ATTEN_DB_12, .channel = SCP_SPIN_ADC_CH,  .unit = ADC_UNIT_1, .bit_width = ADC_OUTPUT_BIT_WIDTH }
	};

	dig_config.pattern_num = ADC_MAX_CH;
	dig_config.adc_pattern = adc_patterns;

	// 4. 설정을 핸들에 반영 및 드라이버 시작
	ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_config));
	ESP_ERROR_CHECK(adc_continuous_start(adc_handle));

#if 0
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SCP_INOUT_ADC_GPIO) 
        				| (1ULL << PLATE_ADC_GPIO)  
        				| (1ULL << MAIN_ADC_GPIO) 
        				| (1ULL << WASTE_ADC_GPIO)
        				| (1ULL << SCP_SPIN_ADC_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
#endif
#if 0
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
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, SCP_SPIN_ADC_CH, &config));
#endif

}

#if 0
static adc_oneshot_unit_handle_t 		adc1_handle = NULL;

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
    else if(mode == SCPSPIN_MOTOR)
    {
	    if (adc_oneshot_read(adc1_handle, SCP_SPIN_ADC_CH, &adc_raw) == ESP_OK) {
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

static bool is_overcurrent_tripped(int mode) {
    float current = read_current_ma(mode);
    
//    ESP_LOGI(TAG, "Driving... Current: %.1fmA, Progress: %lu/%lu", current, g_current_steps, WASTE_TARGET_STEPS);

    if (current > MAX_CURRENT_LIMIT) {
        motor_enable(0);
        ESP_LOGE(TAG, "[TRIP] Overcurrent Detected: %.2f A", current);
        return true;
    }
    return false;
}

#endif

static void current_monitor_task_system(void *arg)
{
    unsigned char result[ADC_READ_LEN] = {0};
    uint32_t ret_num = 0;

    while (1) {
        esp_err_t ret = adc_continuous_read(adc_handle, result, ADC_READ_LEN, &ret_num, pdMS_TO_TICKS(10));
        if (ret == ESP_OK) {
            unsigned int scp_sum_voltage_mv = 0, scp_count = 0;
            unsigned int plate_sum_voltage_mv = 0, plate_count = 0;
            unsigned int main_sum_voltage_mv = 0, main_count = 0;
            unsigned int waste_sum_voltage_mv = 0, waste_count = 0;
            unsigned int scp_spin_sum_voltage_mv = 0, scp_spin_count = 0;

            for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES/*SOC_ADC_DIGI_DATA_BYTES_PER_CONV*/) {
                adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i];
                unsigned int chan = p->type2.channel;
                unsigned int data = p->type2.data & 0xFFF; // 12-bit 마스킹 데이터 추출 안정화

                if (chan == SCP_INOUT_ADC_CH) {
                    unsigned int mv = (data * ADC_VREF_MV) / ADC_MAX_VAL;
                    mv /= INA180A1_GAIN;
                    scp_sum_voltage_mv += mv;
                    scp_count++;
                } else if (chan == PLATE_ADC_CH) {
                    unsigned int mv = (data * ADC_VREF_MV) / ADC_MAX_VAL;
                    mv /= INA180A1_GAIN;
                    plate_sum_voltage_mv += mv;
                    plate_count++;
                } else if (chan == MAIN_ADC_CH) {
                    unsigned int mv = (data * ADC_VREF_MV) / ADC_MAX_VAL;
                    mv /= INA180A1_GAIN;
                    main_sum_voltage_mv += mv;
                    main_count++;
                } else if (chan == WASTE_ADC_CH) {
                    unsigned int mv = (data * ADC_VREF_MV) / ADC_MAX_VAL;
                    mv /= INA180A1_GAIN;
                    waste_sum_voltage_mv += mv;
                    waste_count++;
                } else if (chan == SCP_SPIN_ADC_CH) {
                    unsigned int mv = (data * ADC_VREF_MV) / ADC_MAX_VAL;
                    mv /= INA180A1_GAIN;
                    scp_spin_sum_voltage_mv += mv;
                    scp_spin_count++;
                }
            }

            if (scp_count > 0) {
                unsigned int avg_voltage_mv = scp_sum_voltage_mv / scp_count;
                unsigned int current_ma = avg_voltage_mv / SHUNT_MILI_OHM;

                if (current_ma >= OVERCURRENT_THRESHOLD_MA && !scp_inout_fault) {
                    scp_inout_fault = true;
//                    set_motor_speed_scp_inout(0);
                    ESP_LOGE(TAG, "[SCP FAULT] OVERCURRENT DETECTED! Halted. Current: %ld mA", current_ma);
                }
            }
            if (plate_count > 0) {
                unsigned int avg_voltage_mv = plate_sum_voltage_mv / plate_count;
                unsigned int current_ma = avg_voltage_mv / SHUNT_MILI_OHM;

                if (current_ma >= OVERCURRENT_THRESHOLD_MA && !plate_fault) {
                    plate_fault = true;
//                    set_motor_speed_plate(0);
                    ESP_LOGE(TAG, "[PLATE FAULT] OVERCURRENT DETECTED! Halted. Current: %ld mA", current_ma);
                }
            }
            if (main_count > 0) {
                unsigned int avg_voltage_mv = main_sum_voltage_mv / main_count;
                unsigned int current_ma = avg_voltage_mv / SHUNT_MILI_OHM;

//              ESP_LOGI(TAG, "Current: %ld mA", current_ma);

                if (current_ma >= OVERCURRENT_THRESHOLD_MA && !main_fault) {
                    main_fault = true;
//                    set_motor_speed_plate(0);
                    ESP_LOGE(TAG, "[MAIN FAULT] OVERCURRENT DETECTED! Halted. Current: %ld mA", current_ma);
                }
            }
            if (waste_count > 0) {
                unsigned int avg_voltage_mv = waste_sum_voltage_mv / waste_count;
                unsigned int current_ma = avg_voltage_mv / SHUNT_MILI_OHM;

//				ESP_LOGI(TAG, "Current: %ld mA", current_ma);
				
                if (current_ma >= OVERCURRENT_THRESHOLD_MA && !waste_fault) {
                    waste_fault = true;
//                    set_motor_speed_plate(0);
                    ESP_LOGE(TAG, "[WASTE FAULT] OVERCURRENT DETECTED! Halted. Current: %ld mA", current_ma);
                }
            }
            if (scp_spin_count > 0) {
                unsigned int avg_voltage_mv = scp_spin_sum_voltage_mv / scp_spin_count;
                unsigned int current_ma = avg_voltage_mv / SHUNT_MILI_OHM;

//				ESP_LOGI(TAG, "Current: %ld mA", current_ma);

                if (current_ma >= OVERCURRENT_THRESHOLD_MA && !scp_spin_fault) {
                    scp_spin_fault = true;
//                    set_motor_speed_plate(0);
                    ESP_LOGE(TAG, "[SCP SPIN FAULT] OVERCURRENT DETECTED! Halted. Current: %ld mA", current_ma);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms 주기 모니터링
    }
}

void current_monitor_init(void)
{
    ESP_LOGI(TAG, "%s", __func__);

    init_adc_continuous_system();
    
	xTaskCreate(current_monitor_task_system, "current_monitor_task", 4096, NULL, 5, NULL);

}
