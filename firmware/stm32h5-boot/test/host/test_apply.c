#include "test_common.h"

#define IMG_SZ   (48*1024)


void testApply(void)
{
  printf("[test_apply]\n");

  //-- 정상 적용
  mockFlashReset();
  testMakeImage(FLASH_ADDR_SLOT0, IMG_SZ, 1, 0xD0);

  CHECK(bootApplySlot(0) == OK, "슬롯 적용 성공해야");
  CHECK(bootVerifyFirm() == OK, "적용 후 FIRM 검증 통과");

  {
    boot_slot_info_t firm, s0;

    bootGetFirmInfo(&firm);
    bootGetSlotInfo(0, &s0);
    CHECK(firm.fw_size == IMG_SZ,      "크기 일치");
    CHECK(firm.fw_crc  == s0.fw_crc,   "CRC 일치");
    CHECK(firm.seq     == 1,           "FIRM 에도 seq 가 복사되어야 (%u)", firm.seq);
  }

  //-- 슬롯 무효화
  CHECK(bootInvalidateSlot(0) == OK, "무효화 성공");
  {
    boot_slot_info_t s0;
    bootGetSlotInfo(0, &s0);
    CHECK(s0.valid == false, "무효화 후 valid=false");
  }

  //-- 범위 밖 슬롯
  CHECK(bootApplySlot(FLASH_SLOT_MAX) != OK, "범위 밖 슬롯은 거부");

  //-- 손상된 슬롯 (CRC 불일치)
  mockFlashReset();
  testMakeImage(FLASH_ADDR_SLOT1, IMG_SZ, 1, 0xE0);
  mockFlashPtr(FLASH_ADDR_SLOT1 + FLASH_SIZE_TAG + 100)[0] ^= 0xFF;   // 본문 1바이트 훼손
  CHECK(bootVerifySlot(FLASH_ADDR_SLOT1) == ERR_BOOT_FW_CRC, "훼손 슬롯은 CRC 오류");
  CHECK(bootApplySlot(1) != OK, "훼손 슬롯은 적용 거부");

  CHECK(mockFlashViolations() == 0, "플래시 제약 위반 %u : %s",
        mockFlashViolations(), mockFlashLastViolation());
}
