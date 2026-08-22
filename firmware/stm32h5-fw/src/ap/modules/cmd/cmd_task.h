#ifndef CMD_TASK_H_
#define CMD_TASK_H_


#include "ap_def.h"
#include "cmd.h"

#ifdef _USE_HW_CMD

bool cmdTaskInit(void);
bool cmdTaskUpdate(void);

// 채널 드라이버
extern cmd_driver_t drv_usb_driver;
#if defined(HW_USE_HID) && HW_USE_HID == 1
extern cmd_driver_t drv_hid_driver;
#endif
#ifdef _USE_HW_WIZNET
extern cmd_driver_t drv_tcp_driver;
void drvTcpUpdate(void);
bool drvTcpIsConnected(void);
#endif

// 커맨드 처리
bool cmdBootProcess(cmd_t *p_cmd);
bool cmdBootIsBusy(void);

// cmd 패킷 위의 가상 CLI 채널
//
//   한 응답에 실어 보낼 최대 바이트. cmd 패킷 데이터 상한보다 충분히 작아야 한다.
//   앞에 more 플래그 1바이트가 더 붙는다.
#define CLI_CHUNK_SIZE    512

bool     drvCliInit(void);
bool     drvCliIsConnected(void);
bool     drvCliPutLine(cmd_t *p_cmd, uint8_t *p_data, uint32_t length);
uint32_t drvCliGetOut(uint8_t **pp_data, uint32_t max_len);
bool     drvCliHasMore(void);
void     drvCliEndOut(void);
void     drvCliClearOut(void);
void     drvCliUpdate(void);

#endif

#endif
