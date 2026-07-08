#include "main.h"

static const char *TAG = "LOADCELL";

#define MAIN_CH_CNT           4
#define WASTE_CH_CNT          2
#define TARE_SAMPLES_NEEDED 10 // 100ms 주기이므로 10번 샘플링 = 약 1초 평균

//float scale_factors[MAIN_CH_CNT] = {32.45f, 31.90f, 32.12f, 32.55f}; 
float scale_factors[MAIN_CH_CNT] = {1.0f, 1.0f, 1.0f, 1.0f}; 

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

#if 1
void main_loadcell_sum_weight(short *raw_channels) {
    // 1. 영점 조정 모드 구동 중일 때의 처리
    if (main_taring) {
        for (int i = 0; i < MAIN_CH_CNT; i++) {
            main_tare_buf[i] += raw_channels[i];
        }
        main_tare_cnt++;

        if (main_tare_cnt >= TARE_SAMPLES_NEEDED) {
            for (int i = 0; i < MAIN_CH_CNT; i++) {
                main_tare_off[i] = main_tare_buf[i] / TARE_SAMPLES_NEEDED;
            }
            main_taring = false;
            ESP_LOGI(TAG, ">> main loadcell taring finished << [CH1:%ld, CH2:%ld, CH3:%ld, CH4:%ld]", 
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

//    ESP_LOGI(TAG, "main net weight %.2f g [CH1]: %.1f g, [CH2]: %.1f g, [CH3]: %.1f g, [CH4]: %.1f g", 
//             total_summed_weight, 
//             each_calibrated_weight[0], 
//             each_calibrated_weight[1], 
//             each_calibrated_weight[2], 
//             each_calibrated_weight[3]);
}

void waste_loadcell_sum_weight(short *raw_channels) {
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
            ESP_LOGI(TAG, ">> waste loadcell taring finished << [CH1:%ld, CH2:%ld]", 
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

#else
static void process_loadcell_data(short *raw_channels) {
    // 1. 영점 조정 모드일 때의 처리
    if (main_taring) {
        for (int i = 0; i < MAIN_CH_CNT; i++) {
            main_tare_buf[i] += raw_channels[i];
        }
        main_tare_cnt++;

        if (main_tare_cnt >= TARE_SAMPLES_NEEDED) {
            for (int i = 0; i < MAIN_CH_CNT; i++) {
                main_tare_off[i] = main_tare_buf[i] / TARE_SAMPLES_NEEDED;
            }
            main_taring = false;
            ESP_LOGI(TAG, "finished taring! Offsets -> CH1:%ld, CH2:%ld, CH3:%ld, CH4:%ld", 
                     main_tare_off[0], main_tare_off[1], main_tare_off[2], main_tare_off[3]);
        }
        return; // skip counting weight while taring.
    }

    // 2. 일반 모드: 실제 무게 계산 및 합산
    float each_weight[MAIN_CH_CNT] = {0.0f};
    float total_weight = 0.0f;

    for (int i = 0; i < MAIN_CH_CNT; i++) {
        // (현재 Raw값 - 영점 오프셋) / 보정 계수
        each_weight[i] = (float)(raw_channels[i] - main_tare_off[i]) / scale_factors[i];
        total_weight += each_weight[i];
    }

    // 결과 출력 (100ms마다 실행됨)
    ESP_LOGI(TAG, "Total: %.2fg | CH1:%.1fg, CH2:%.1fg, CH3:%.1fg, CH4:%.1fg", 
             total_weight, each_weight[0], each_weight[1], each_weight[2], each_weight[3]);
}
#endif

void loadcell_proc(int *values)
{
	short load[MAIN_CH_CNT+WASTE_CH_CNT];
	int i = 0;
	load[i++] = (short)*values++;
	load[i++] = (short)*values++;
	load[i++] = (short)*values++;
	load[i++] = (short)*values++;
	load[i++] = (short)*values++;
	load[i++] = (short)*values++;
	main_loadcell_sum_weight(&load[0]);
	waste_loadcell_sum_weight(&load[4]);
}

void loadcell_init(void) 
{
	ESP_LOGI(TAG, "%s", __func__);
    main_loadcell_tare();
    waste_loadcell_tare();
}
