/*
 * hid.h
 *
 *   vendor-defined HID 채널의 hw 계층 래퍼.
 *   상위(ap/modules/cmd/driver/drv_hid.c)는 이 인터페이스만 본다.
 */

#ifndef HID_H_
#define HID_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"

#if defined(_USE_HW_USB) && (HW_USE_HID == 1)


#define HID_PACKET_SIZE       64


bool     hidInit(void);
bool     hidIsConnect(void);
bool     hidIsReady(void);
uint32_t hidWrite(uint8_t *p_data, uint32_t length);


#endif

#ifdef __cplusplus
}
#endif

#endif /* HID_H_ */
