#ifndef UTIL_CORE_H_
#define UTIL_CORE_H_


#ifdef __cplusplus
 extern "C" {
#endif


#include "def.h"


uint32_t utilConvert8ToU32 (uint8_t *p_data);
uint16_t utilConvert8ToU16 (uint8_t *p_data);

void     utilUpdateCrc(uint16_t *p_crc_cur, uint8_t data_in);
uint16_t utilCalcCRC(uint16_t crc_cur, uint8_t *p_data, uint32_t length);

//-- 달력 <-> epoch(1970-01-01 기준 초).
//
//   Howard Hinnant 의 days_from_civil / civil_from_days 다. 윤년·윤일 처리가
//   분기 없이 끝나서 짧고 틀릴 여지가 적다. RTC 드라이버와 부트 이벤트 로그가
//   같은 함수를 쓰고, 호스트 유닛 테스트도 이 구현을 그대로 시험한다.
//
uint32_t utilEpochFromCivil(uint16_t year, uint8_t month, uint8_t day,
                            uint8_t hour, uint8_t min, uint8_t sec);
bool     utilCivilFromEpoch(uint32_t epoch, uint16_t *p_year, uint8_t *p_month,
                            uint8_t *p_day, uint8_t *p_hour, uint8_t *p_min, uint8_t *p_sec);

#ifdef __cplusplus
}
#endif


#endif 