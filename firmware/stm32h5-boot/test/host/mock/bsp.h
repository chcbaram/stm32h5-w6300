#ifndef BSP_H_
#define BSP_H_
// 호스트 테스트용 bsp.h 대체. include 경로에서 실제 bsp.h 보다 앞에 온다.
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "def.h"

// hw_def.h 가 쓰는 HAL 상수
#define RTC_BKP_DR3   3
#define RTC_BKP_DR4   4
#define RTC_BKP_DR5   5
#define RTC_BKP_DR6   6
#define RTC_BKP_DR7   7
#define RTC_BKP_DR8   8

#define FLASH_SECTOR_SIZE   0x2000U

void     logPrintf(const char *fmt, ...);
void     delay(uint32_t ms);
uint32_t millis(void);
bool     bspDeInit(void);

#endif
