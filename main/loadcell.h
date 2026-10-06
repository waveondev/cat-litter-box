#ifndef __LOADCELL_H__
#define __LOADCELL_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 고양이 진입/이탈 감지 상태 정의
typedef enum {
    CAT_STATE_INIT = 0,     // 부팅 / 청소 후 1분간 초기 Baseline 수집 중
    CAT_STATE_IDLE,         // 대기 중 (200개 링 버퍼 Baseline 추적)
    CAT_STATE_OCCUPIED,     // 고양이 탑승 중 (청소 절대 금지)
    CAT_STATE_WAIT_CLEAN    // 고양이 이탈 후 5분 카운트다운 진행 중
} cat_detect_state_t;

/* Function Prototypes -------------------------------------------------------*/
double get_weight(int mode);
void send_loadcell_msg(void *message, uint32_t cmd);
void loadcell_proc(int *values);
void loadcell_init(void);

// 상태 및 Baseline 확인 지원 함수
cat_detect_state_t get_cat_detect_state(void);
float get_cat_baseline_weight(void);

#ifdef __cplusplus
}
#endif

#endif /* __LOADCELL_H__ */