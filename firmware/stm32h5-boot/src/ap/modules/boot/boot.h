#ifndef BOOT_H_
#define BOOT_H_


#include "ap_def.h"


#define BOOT_SLOT_MAGIC       0x534C4F54UL      // "SLOT"

#define BOOT_SLOT_FLAG_NONE   0xFFFFFFFFUL
#define BOOT_SLOT_TAG_OFFSET  0x020            // firm_tag_t 뒤, 슬롯 메타데이터
#define BOOT_SLOT_INV_OFFSET  0x030            // 무효화 마커
#define BOOT_SLOT_INV_MAGIC   0x494E564CUL      // "INVL"


//-- 슬롯 TAG 영역 오프셋 0x20 에 놓이는 16바이트 = 정확히 1 쿼드워드.
//   firm_tag_t(def.h)는 다른 프로젝트와 공유하므로 건드리지 않는다.
//
typedef struct
{
  uint32_t magic;
  uint32_t seq;
  uint32_t flags;
  uint32_t crc;
} boot_slot_t;


typedef struct
{
  uint8_t  index;
  uint32_t addr;
  bool     valid;
  uint32_t seq;
  uint32_t fw_size;
  uint32_t fw_crc;
} boot_slot_info_t;


//-- 부트 이벤트 로그 (0x081E0000, 8KB 섹터 2개 핑퐁)
//
//   전원을 뽑아도 남아야 하므로 플래시에 둔다. 정상 부팅은 기록하지 않고
//   아래 4개 이벤트(상태 변화)만 남긴다.
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


bool     bootInit(void);

uint16_t bootVerifySlot(uint32_t addr);
uint16_t bootVerifyFirm(void);

bool     bootGetSlotInfo(uint8_t index, boot_slot_info_t *p_info);
bool     bootGetFirmInfo(boot_slot_info_t *p_info);
uint32_t bootGetSlotAddr(uint8_t index);

int8_t   bootGetWriteSlot(void);
int8_t   bootGetPendingSlot(void);
int8_t   bootGetRollbackSlot(void);
uint32_t bootGetNextSeq(void);

uint16_t bootApplySlot(uint8_t index);
uint16_t bootInvalidateSlot(uint8_t index);

uint16_t bootJumpFirm(void);


#endif
