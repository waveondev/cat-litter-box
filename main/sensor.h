#ifndef __SENSOR_H__
#define __SENSOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#define PT_BIT_SCP_SPIN_ST	0x0001
#define PT_BIT_SCP_OUT		0x0002
#define PT_BIT_WASTE_CLOSE	0x0004
#define PT_BIT_MCOVER_OPEN	0x0008
#define PT_BIT_SCP_SPIN_ED	0x0010
#define PT_BIT_SCP_IN		0x0020
#define PT_BIT_WASTE_OPEN	0x0040
#define PT_BIT_REED_SW		0x0080
#define PT_BIT_MCOVER_CLOSE	0x0100

/* Function Prototypes -------------------------------------------------------*/
bool get_sensor_enable(void);

int set_pt_status(int value);
int get_pt_status(void);

// 🌟 TOF 센서 관련 함수 프로토타입
int set_tof_sensor_enable(bool enable);
bool get_tof_sensor_enable(void);
int get_tof_distance(void); 
void reset_tof_ring_buffer(void); // 검사 직전 이전 버퍼 잔류 수치 전면 삭제(Clear)
int get_tof_ring_count(void);     // 🌟 [누락 해결]: motor.c 연동을 위한 TOF 링버퍼 카운트 함수 추가

int sensor_data_parser(char *input);
void sensor_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_H__ */