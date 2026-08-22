#ifndef INCLUDE_RTC_H_
#define INCLUDE_RTC_H_


#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"


#ifdef _USE_HW_RTC

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


bool rtcInit(void);
bool rtcGetInfo(rtc_info_t *rtc_info);
bool rtcGetTime(rtc_time_t *rtc_time);
bool rtcGetDate(rtc_date_t *rtc_date);
bool rtcSetTime(rtc_time_t *rtc_time);
bool rtcSetDate(rtc_date_t *rtc_date);

//-- epoch(1970-01-01 UTC 기준 초) 변환.
//
//   보드에 코인셀이 없어 전원을 뽑으면 RTC 가 초기화된다. 그래서 "시각을 아는가"
//   를 늘 함께 다뤄야 한다. rtcGetEpoch() 은 모르면 0 을 돌려주고, 부트 이벤트
//   로그와 호스트(웹/툴)는 0 을 "시각 없음" 으로 취급한다.
//
//   맞춘 적 없는 RTC 는 리셋 기본값(2000년대 초)에 머물기 때문에, 연도가
//   RTC_EPOCH_YEAR_MIN 보다 이르면 모르는 것으로 본다.
//
#define RTC_EPOCH_YEAR_MIN    2024

uint32_t rtcGetEpoch(void);
bool     rtcSetEpoch(uint32_t epoch);
bool     rtcEpochToInfo(uint32_t epoch, rtc_info_t *p_info);

bool rtcSetReg(uint32_t index, uint32_t data);
bool rtcGetReg(uint32_t index, uint32_t *p_data);

#endif

#ifdef __cplusplus
}
#endif

#endif 