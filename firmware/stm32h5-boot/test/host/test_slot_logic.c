#include "test_common.h"

#define IMG_SZ   (32*1024)


//-- 슬롯 선택 규칙 진리표.
//
//   핑퐁의 핵심은 별도의 활성 슬롯 포인터 없이
//   FIRM 의 (fw_size, fw_crc) 와 seq 만으로 역할이 결정된다는 것이다.
//
void testSlotLogic(void)
{
  printf("[test_slot_logic]\n");

  //-- 1. 완전히 빈 상태
  mockFlashReset();
  CHECK(bootGetWriteSlot()    == 0,  "빈 상태 write slot 은 0 이어야");
  CHECK(bootGetPendingSlot()  == -1, "빈 상태 pending 없음");
  CHECK(bootGetRollbackSlot() == -1, "빈 상태 rollback 없음");

  //-- 2. FIRM 만 있고 슬롯이 비었을 때 (SWD 로 직접 구운 상태)
  mockFlashReset();
  testMakeImage(FLASH_ADDR_FIRM, IMG_SZ, 0, 0xA0);   // seq 0 = boot_slot_t 없음과 같음
  mockFlashPtr(FLASH_ADDR_FIRM)[BOOT_SLOT_TAG_OFFSET] = 0xFF;  // seq 메타 제거
  CHECK(bootVerifyFirm() == OK,      "FIRM 검증 통과해야");
  CHECK(bootGetRollbackSlot() == -1, "FIRM seq 가 없으면 롤백 금지");

  //-- 3. FIRM = v2(seq2), SLOT0 = v2(백업본), SLOT1 = v1(낡음)
  mockFlashReset();
  testMakeImage(FLASH_ADDR_FIRM,  IMG_SZ, 2, 0xB0);
  testMakeImage(FLASH_ADDR_SLOT0, IMG_SZ, 2, 0xB0);
  testMakeImage(FLASH_ADDR_SLOT1, IMG_SZ, 1, 0xA0);
  CHECK(bootGetWriteSlot()    == 1, "FIRM 과 일치하는 SLOT0 은 보존, 낡은 SLOT1 에 쓴다");
  CHECK(bootGetPendingSlot()  == -1, "FIRM 보다 오래된 슬롯은 적용 대기가 아니다");
  CHECK(bootGetRollbackSlot() == 1, "롤백 대상은 SLOT1(v1)");
  CHECK(bootGetNextSeq()      == 3, "next seq 는 3");

  //-- 4. 새 이미지가 SLOT1 에 들어온 직후 (아직 FIRM 미적용)
  testMakeImage(FLASH_ADDR_SLOT1, IMG_SZ, 3, 0xC0);
  CHECK(bootGetPendingSlot()  == 1, "seq 가 FIRM 보다 큰 SLOT1 이 적용 대기");
  CHECK(bootGetRollbackSlot() == -1, "적용 전이므로 되돌아갈 옛 이미지가 없다");

  //-- 5. 무효 슬롯이 있으면 그쪽을 최우선으로 쓴다
  mockFlashReset();
  testMakeImage(FLASH_ADDR_FIRM,  IMG_SZ, 2, 0xB0);
  testMakeImage(FLASH_ADDR_SLOT1, IMG_SZ, 2, 0xB0);
  CHECK(bootGetWriteSlot() == 0, "무효 SLOT0 이 1순위");

  //-- 6. 두 슬롯이 같은 이미지면 seq 가 작은 쪽에 쓴다
  mockFlashReset();
  testMakeImage(FLASH_ADDR_FIRM,  IMG_SZ, 5, 0xB0);
  testMakeImage(FLASH_ADDR_SLOT0, IMG_SZ, 5, 0xB0);
  testMakeImage(FLASH_ADDR_SLOT1, IMG_SZ, 4, 0xB0);
  CHECK(bootGetWriteSlot() == 1, "둘 다 FIRM 과 같으면 seq 작은 쪽");

  CHECK(mockFlashViolations() == 0, "플래시 제약 위반 %u : %s",
        mockFlashViolations(), mockFlashLastViolation());
}
