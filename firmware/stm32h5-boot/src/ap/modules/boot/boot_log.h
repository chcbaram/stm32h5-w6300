#ifndef BOOT_LOG_H_
#define BOOT_LOG_H_


#include "ap_def.h"


//-- 부트 이벤트 로그 (FLASH_ADDR_BOOT_LOG, 8KB 섹터 2개 핑퐁)
//
//   전원을 뽑아도 남아야 하므로 플래시에 둔다. 정상 부팅은 기록하지 않고
//   아래 이벤트(상태 변화)만 남긴다.
//
//   컴팩션 없이 두 섹터를 번갈아 쓴다. 한쪽이 차면 반대쪽을 지우고 그쪽에
//   append 하며, 이전 세대는 그대로 보존된다. 단일 섹터 순환은 섹터가 찰 때마다
//   이력을 통째로 잃고, 소거 도중 전원이 끊기면 ECC 오류까지 난다.
//
#define BOOT_LOG_MAGIC        0x424C4F47UL     // "BLOG"
#define BOOT_LOG_SECTOR_MAX   2
#define BOOT_LOG_REC_SIZE     32
#define BOOT_LOG_REC_MAX      (FLASH_SECTOR_SIZE / BOOT_LOG_REC_SIZE)   // 256


typedef enum
{
  BOOT_EVT_UPDATE = 1,        // 슬롯 -> FIRM 정상 적용
  BOOT_EVT_ROLLBACK,          // boot_try_cnt 초과로 롤백
  BOOT_EVT_FAULT_RECOVER,     // fault_cnt 초과로 롤백
  BOOT_EVT_VERIFY_FAIL,       // 검증/적용 실패
  BOOT_EVT_ECC_CLEAN,         // ECC 2비트 오류 영역 정리
} boot_evt_t;


typedef struct
{
  uint32_t magic;
  uint32_t seq;
  uint8_t  event;
  uint8_t  slot;
  uint16_t reset_bits;
  uint32_t timestamp;
  uint32_t from_crc;
  uint32_t to_crc;
  uint32_t fault_pc;
  uint32_t crc;
} boot_log_t;                 // 32B = 2 쿼드워드


bool     bootLogInit(void);
bool     bootLogWrite(boot_evt_t evt, int8_t slot, uint32_t from_crc,
                      uint32_t to_crc, uint32_t fault_pc);
uint16_t bootLogGetCount(void);
bool     bootLogRead(uint16_t idx, boot_log_t *p_log);
bool     bootLogClear(void);


#endif
