#include "main.h"

static const char *TAG = "CURRENT_MON";

#define ADC_MAX_CH              5
#define ADC_PERIOD_MS           10
#define ADC_READ_TIMEOUT_MS     5   

#define SCP_INOUT_ADC_CH        ADC_CHANNEL_2
#define PLATE_ADC_CH            ADC_CHANNEL_1
#define MAIN_ADC_CH             ADC_CHANNEL_3
#define WASTE_ADC_CH            ADC_CHANNEL_9
#define SCP_SPIN_ADC_CH         ADC_CHANNEL_6

static adc_continuous_handle_t  adc_handle = NULL;
static unsigned char current_fault_status = 0;

#define ADC_OUTPUT_BIT_WIDTH        (SOC_ADC_DIGI_MAX_BITWIDTH) 
#define ADC_READ_LEN                1024

static void init_adc_continuous_system(void)
{
    ESP_LOGI(TAG, "%s", __func__);

    adc_continuous_handle_cfg_t adc_config = {
     .max_store_buf_size = 6000,
     .conv_frame_size = ADC_READ_LEN, 
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_continuous_config_t dig_config = {
     .sample_freq_hz = 50 * 1000,
     .conv_mode = ADC_CONV_SINGLE_UNIT_1,
     .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };

    static adc_digi_pattern_config_t adc_patterns[ADC_MAX_CH] = {
     { .atten = ADC_ATTEN_DB_12, .channel = SCP_INOUT_ADC_CH, .unit = ADC_UNIT_1, .bit_width = ADC_OUTPUT_BIT_WIDTH },
     { .atten = ADC_ATTEN_DB_12, .channel = PLATE_ADC_CH,     .unit = ADC_UNIT_1, .bit_width = ADC_OUTPUT_BIT_WIDTH },
     { .atten = ADC_ATTEN_DB_12, .channel = MAIN_ADC_CH,      .unit = ADC_UNIT_1, .bit_width = ADC_OUTPUT_BIT_WIDTH },
     { .atten = ADC_ATTEN_DB_12, .channel = WASTE_ADC_CH,     .unit = ADC_UNIT_1, .bit_width = ADC_OUTPUT_BIT_WIDTH },
     { .atten = ADC_ATTEN_DB_12, .channel = SCP_SPIN_ADC_CH,  .unit = ADC_UNIT_1, .bit_width = ADC_OUTPUT_BIT_WIDTH }
    };

    dig_config.pattern_num = ADC_MAX_CH;
    dig_config.adc_pattern = adc_patterns;

    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_config));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
}

static void current_monitor_task(void *pvParameters)
{
    int m1_cur, m2_cur, m3_cur, m4_cur, m5_cur;
    
    app_config_t *app = get_app_config();
    m1_cur = (int)app->m1_jam_current;
    m2_cur = (int)app->m2_jam_current;
    m3_cur = (int)app->m3_jam_current;
    m4_cur = (int)app->m4_jam_current;
    m5_cur = (int)app->m5_jam_current;
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(ADC_PERIOD_MS);
    static uint8_t result[ADC_READ_LEN];

    unsigned int scpinout_ma = 0, plate_ma = 0, main_ma = 0, waste_ma = 0, scpspin_ma = 0;

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        uint32_t ret_num = 0;
        esp_err_t ret = adc_continuous_read(adc_handle, result, ADC_READ_LEN, &ret_num, pdMS_TO_TICKS(ADC_READ_TIMEOUT_MS));
        
        if (ret == ESP_OK && ret_num > 0) {
            uint64_t scp_sum_raw = 0;       uint32_t scp_count = 0;
            uint64_t plate_sum_raw = 0;     uint32_t plate_count = 0;
            uint64_t main_sum_raw = 0;      uint32_t main_count = 0;
            uint64_t waste_sum_raw = 0;     uint32_t waste_count = 0;
            uint64_t scp_spin_sum_raw = 0;  uint32_t scp_spin_count = 0;

            for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i];
                uint32_t chan = p->type2.channel;
                uint32_t raw_data = p->type2.data & 0xFFF;

                if (chan == SCP_INOUT_ADC_CH) { scp_sum_raw += raw_data; scp_count++; }
                else if (chan == PLATE_ADC_CH) { plate_sum_raw += raw_data; plate_count++; }
                else if (chan == MAIN_ADC_CH) { main_sum_raw += raw_data; main_count++; }
                else if (chan == WASTE_ADC_CH) { waste_sum_raw += raw_data; waste_count++; }
                else if (chan == SCP_SPIN_ADC_CH) { scp_spin_sum_raw += raw_data; scp_spin_count++; }
            }

            /* [수정] 전류(mA) 계산 로직 정밀화 및 버그 수정 */
            if (scp_count > 0) {
                scpinout_ma = (unsigned int)(((scp_sum_raw / scp_count) * 330) / 819);
                if(scpinout_ma > m2_cur) current_fault_status |= SCP_INOUT_FAULT;
            }
            if (plate_count > 0) {
                plate_ma = (unsigned int)(((plate_sum_raw / plate_count) * 330) / 819);
                if(plate_ma > m1_cur) current_fault_status |= PLATE_FAULT;
            }
            if (main_count > 0) {
                main_ma = (unsigned int)(((main_sum_raw / main_count) * 330) / 819);
                if(main_ma > m4_cur) current_fault_status |= MAIN_FAULT;
            }
            if (waste_count > 0) {
                waste_ma = (unsigned int)(((waste_sum_raw / waste_count) * 330) / 819);
                if(waste_ma > m5_cur) current_fault_status |= WASTE_FAULT;
            }
            if (scp_spin_count > 0) {
                scpspin_ma = (unsigned int)(((scp_spin_sum_raw / scp_spin_count) * 330) / 819);
                if(scpspin_ma > m3_cur) current_fault_status |= SCP_SPIN_FAULT;
            }
        }
    }
}

unsigned char get_current_fault(void) { return current_fault_status; }

void current_monitor_init(void)
{
    ESP_LOGI(TAG, "%s", __func__);
    current_fault_status = 0;
    init_adc_continuous_system();
    xTaskCreate(current_monitor_task, "current_monitor_task", 4096, NULL, 5, NULL);
}
