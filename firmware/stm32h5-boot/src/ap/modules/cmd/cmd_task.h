#ifndef CMD_TASK_H_
#define CMD_TASK_H_


#include "ap_def.h"
#include "cmd.h"

#ifdef _USE_HW_CMD

bool cmdTaskInit(void);
bool cmdTaskUpdate(void);

// 채널 드라이버
extern cmd_driver_t cmd_usb_driver;
#if defined(HW_USE_HID) && HW_USE_HID == 1 && defined(CMD_USE_HID)
extern cmd_driver_t cmd_hid_driver;
#endif

// 커맨드 처리
bool cmdBootProcess(cmd_t *p_cmd);
bool cmdBootIsBusy(void);

#endif

#endif
