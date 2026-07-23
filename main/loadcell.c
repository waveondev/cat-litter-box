#include "main.h"

static const char *TAG = "LOADCELL";

#define MAIN_CH_CNT           	4
#define WASTE_CH_CNT          	2
#define TARE_SAMPLES_NEEDED 	10
#define MAIN_SCALE_FACT			1000.0f

float scale_factors[MAIN_CH_CNT] = {32.45f, 31.90f, 32.12f, 32.55f}; 

int32_t main_tare_off[MAIN_CH_CNT] = {0, 0, 0, 0};
volatile bool main_taring = false;
int32_t main_tare_buf[MAIN_CH_CNT] = {0};
int main_tare_cnt = 0;

int32_t waste_tare_off[WASTE_CH_CNT] = {0, 0};
volatile bool waste_taring = false;
int32_t waste_tare_buf[WASTE_CH_CNT] = {0};
int waste_tare_cnt = 0;

static void main_loadcell_tare(void) {
    if (main_taring) return;
    
    memset(main_tare_buf, 0, sizeof(main_tare_buf));
    main_tare_cnt = 0;
    main_taring = true; // 영점 계산 시작 플래그 ON
    ESP_LOGI(TAG, "Empty main loadcell, start to tare zero .......");
}

static void waste_loadcell_tare(void) {
    if (waste_taring) return;
    
    memset(waste_tare_buf, 0, sizeof(waste_tare_buf));
    waste_tare_cnt = 0;
    waste_taring = true; // 영점 계산 시작 플래그 ON
    ESP_LOGI(TAG, "Empty waste loadcell, start to tare zero .......");
}

void main_loadcell_sum_weight(int *raw_channels) {
    // 1. 영점 조정 모드 구동 중일 때의 처리
    if (main_taring) {
        for (int i = 0; i < MAIN_CH_CNT; i++) {
            main_tare_buf[i] += raw_channels[i];
//            ESP_LOGI(TAG, ">> accumulate tare data %d %d %d ", i, main_tare_buf[i], raw_channels[i]);
        }
        main_tare_cnt++;

        if (main_tare_cnt >= TARE_SAMPLES_NEEDED) {
            for (int i = 0; i < MAIN_CH_CNT; i++) {
                main_tare_off[i] = main_tare_buf[i] / TARE_SAMPLES_NEEDED;
            }
            main_taring = false;
            ESP_LOGI(TAG, ">> main loadcell taring finished CH1:%ld, CH2:%ld, CH3:%ld, CH4:%ld", 
                     main_tare_off[0], main_tare_off[1], main_tare_off[2], main_tare_off[3]);
        }
        return; // skip counting weight while taring 
    }

    
    float each_calibrated_weight[MAIN_CH_CNT] = {0.0f};
    float total_summed_weight = 0.0f;

    for (int i = 0; i < MAIN_CH_CNT; i++) {
        int32_t net_raw = (int32_t)raw_channels[i] - main_tare_off[i];
        
        each_calibrated_weight[i] = (float)net_raw / scale_factors[i];
    }

    for (int i = 0; i < MAIN_CH_CNT; i++) {
        total_summed_weight += each_calibrated_weight[i];
    }

    if (total_summed_weight < 0.3f && total_summed_weight > -0.3f) {
        total_summed_weight = 0.0f;
    }
    ESP_LOGI(TAG, "main net weight %.2f g [CH1]: %.1f g, [CH2]: %.1f g, [CH3]: %.1f g, [CH4]: %.1f g", 
             total_summed_weight, 
             each_calibrated_weight[0], 
             each_calibrated_weight[1], 
             each_calibrated_weight[2], 
             each_calibrated_weight[3]);
}

void waste_loadcell_sum_weight(int *raw_channels) {
    // 1. 영점 조정 모드 구동 중일 때의 처리
    if (waste_taring) {
        for (int i = 0; i < WASTE_CH_CNT; i++) {
            waste_tare_buf[i] += raw_channels[i];
        }
        waste_tare_cnt++;

        if (waste_tare_cnt >= TARE_SAMPLES_NEEDED) {
            for (int i = 0; i < WASTE_CH_CNT; i++) {
                waste_tare_off[i] = waste_tare_buf[i] / TARE_SAMPLES_NEEDED;
            }
            waste_taring = false;
            ESP_LOGI(TAG, ">> waste loadcell taring finished CH1:%ld, CH2:%ld", 
                     waste_tare_off[0], waste_tare_off[1]);
        }
        return; // skip counting weight while taring 
    }

    float each_calibrated_weight[MAIN_CH_CNT] = {0.0f};
    float total_summed_weight = 0.0f;

    for (int i = 0; i < WASTE_CH_CNT; i++) {
        int32_t net_raw = (int32_t)raw_channels[i] - waste_tare_off[i];
        
        each_calibrated_weight[i] = (float)net_raw / scale_factors[i];
    }

    // 3. 4개 지점 소프트웨어 Summing (최종 합산)
    for (int i = 0; i < WASTE_CH_CNT; i++) {
        total_summed_weight += each_calibrated_weight[i];
    }

    // 4. 무부하 상태 미세 노이즈 제거 (Deadband 처리)
    // 센서 흔들림으로 인해 아무것도 없을 때 0.1g, -0.2g 변동하는 현상 방지
    if (total_summed_weight < 0.3f && total_summed_weight > -0.3f) {
        total_summed_weight = 0.0f;
    }

    // 5. 최종 데이터 출력 (100ms 주기 연산 결과)
//    ESP_LOGI(TAG, "waste net weight %.2f g [CH1]: %.1f g, [CH2]: %.1f g", 
//             total_summed_weight, 
//             each_calibrated_weight[0], 
//             each_calibrated_weight[1]);
}

#define LOADCELL_SCALE			1

void loadcell_proc(int *values)
{
	int load[MAIN_CH_CNT+WASTE_CH_CNT];
	int i = 0;

	if(!get_sensor_enable())
		return;
	
	load[i++] = (int)*values++;
	load[i++] = (int)*values++;
	load[i++] = (int)*values++;
	load[i++] = (int)*values++;
	load[i++] = (int)*values++;
	load[i++] = (int)*values++;
	i = 0;
	load[i++] /= LOADCELL_SCALE;
	load[i++] /= LOADCELL_SCALE;
	load[i++] /= LOADCELL_SCALE;
	load[i++] /= LOADCELL_SCALE;
	load[i++] /= LOADCELL_SCALE;
	load[i++] /= LOADCELL_SCALE;

	for(i=0;i<(MAIN_CH_CNT+WASTE_CH_CNT);i++)
	{
		if(load[i] < 0)
		{
			load[i] *= -1;
		}
	}
	
	main_loadcell_sum_weight(&load[0]);
	waste_loadcell_sum_weight(&load[4]);
}

void loadcell_init(void) 
{
	ESP_LOGI(TAG, "%s", __func__);
    main_loadcell_tare();
    waste_loadcell_tare();
}
