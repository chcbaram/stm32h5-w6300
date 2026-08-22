#ifndef BOOT_H_
#define BOOT_H_


#include "ap_def.h"
#include "boot_log.h"


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
