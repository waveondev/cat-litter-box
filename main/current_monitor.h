#ifndef __CURRENT_MONITOR_H__
#define __CURRENT_MONITOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#define SCP_INOUT_FAULT				0x01
#define PLATE_FAULT					0x02
#define MAIN_FAULT					0x04
#define WASTE_FAULT					0x08
#define SCP_SPIN_FAULT				0x10


/* Includes ------------------------------------------------------------------*/

void current_monitor_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __CURRENT_MONITOR_H__ */

