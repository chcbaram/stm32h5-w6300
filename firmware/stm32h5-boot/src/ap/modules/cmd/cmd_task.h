#ifndef CMD_TASK_H_
#define CMD_TASK_H_


#include "ap_def.h"
#include "cmd.h"

#ifdef _USE_HW_CMD

bool cmdTaskInit(void);
bool cmdTaskUpdate(void);

// 채널 드라이버
extern cmd_driver_t cmd_usb_driver;
#if defined(HW_USE_HID) && HW_USE_HID == 1
extern cmd_driver_t cmd_hid_driver;
#endif

// 커맨드 처리
bool cmdBootProcess(cmd_t *p_cmd);
bool cmdBootIsBusy(void);

// cmd 패킷 위의 가상 CLI 채널
bool     cliCmdInit(void);
bool     cliCmdIsConnected(void);
bool     cliCmdPutLine(cmd_t *p_cmd, uint8_t *p_data, uint32_t length);
uint32_t cliCmdGetOut(uint8_t **pp_data);
void     cliCmdClearOut(void);

#endif

#endif
