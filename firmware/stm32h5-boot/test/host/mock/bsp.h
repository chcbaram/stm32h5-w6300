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

//-- RTC. boot_log.c 가 기록 시각을 채울 때 쓴다.
//   호스트 시험에서는 mockRtcSet() 으로 원하는 시각을 넣는다.
typedef struct
{
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} rtc_time_t;

typedef struct
{
  uint8_t year;
  uint8_t month;
  uint8_t day;
  uint8_t week;
} rtc_date_t;

typedef struct
{
  rtc_time_t time;
  rtc_date_t date;
} rtc_info_t;

#define RTC_EPOCH_YEAR_MIN    2024

bool     rtcGetInfo(rtc_info_t *p_info);
uint32_t rtcGetEpoch(void);
void     mockRtcSet(int year, int month, int day, int h, int m, int s);

#define FLASH_SECTOR_SIZE   0x2000U

void     logPrintf(const char *fmt, ...);
void     delay(uint32_t ms);
uint32_t millis(void);
bool     bspDeInit(void);

#endif
