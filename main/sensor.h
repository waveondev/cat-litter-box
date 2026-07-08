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

/* Includes ------------------------------------------------------------------*/

int set_pt_status(int value);
int get_pt_status(void);

int sensor_data_parser(char *input);
void sensor_init(void);
#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_H__ */
