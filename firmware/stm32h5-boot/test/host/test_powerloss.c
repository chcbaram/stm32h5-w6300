#include "test_common.h"

#define IMG_SZ   (24*1024)


//-- 부팅 가능성 불변식.
//
//   어떤 중단 지점에서도 "FIRM 또는 슬롯 중 최소 하나는 유효" 해야 한다.
//   그래야 다음 부팅에서 복구할 수 있다.
//
static bool invariantHolds(void)
{
  if (bootVerifyFirm() == OK)
    return true;

  for (uint8_t i = 0; i < FLASH_SLOT_MAX; i++)
  {
    if (bootVerifySlot(bootGetSlotAddr(i)) == OK)
      return true;
  }
  return false;
}

//-- "TAG 는 유효한데 본문이 미완성" 상태가 절대 없어야 한다.
//
//   bootVerifySlot() 이 TAG 매직 -> 크기 -> CRC 순으로 보므로, 커밋 순서가
//   틀리면 이 함수가 ERR_BOOT_FW_CRC 를 돌려준다. 즉 CRC 오류 자체가
//   "커밋 순서가 잘못됐다" 는 신호다.
//
static bool noHalfCommitted(void)
{
  if (bootVerifyFirm() == ERR_BOOT_FW_CRC)
    return false;

  for (uint8_t i = 0; i < FLASH_SLOT_MAX; i++)
  {
    if (bootVerifySlot(bootGetSlotAddr(i)) == ERR_BOOT_FW_CRC)
      return false;
  }
  return true;
}


//-- bootApplySlot() 의 모든 중단 지점을 전수로 돌린다.
//
//   실기에서는 전원 차단 타이밍을 제어할 수 없어 운에 맡겨야 하지만,
//   목에서는 n 번째 플래시 연산 직후를 정확히 노릴 수 있다.
//
static void testApplyPowerLoss(void)
{
  uint32_t total_ops;
  int bad_invariant = 0, bad_half = 0, no_recover = 0;

  // 먼저 정상 실행으로 전체 연산 수를 센다.
  mockFlashReset();
  testMakeImage(FLASH_ADDR_SLOT0, IMG_SZ, 1, 0xD0);
  testMakeImage(FLASH_ADDR_SLOT1, IMG_SZ, 2, 0xE0);
  testMakeImage(FLASH_ADDR_FIRM,  IMG_SZ, 1, 0xD0);
  mockFlashOpCountReset();
  bootApplySlot(1);
  total_ops = mockFlashOpCount();

  printf("  bootApplySlot() 플래시 연산 %u회 -> 전 지점 시뮬레이션\n", total_ops);

  for (uint32_t n = 0; n < total_ops; n++)
  {
    // 초기 상태 : FIRM=v1(seq1), SLOT0=v1(백업본), SLOT1=v2(새 이미지)
    mockFlashReset();
    testMakeImage(FLASH_ADDR_FIRM,  IMG_SZ, 1, 0xD0);
    testMakeImage(FLASH_ADDR_SLOT0, IMG_SZ, 1, 0xD0);
    testMakeImage(FLASH_ADDR_SLOT1, IMG_SZ, 2, 0xE0);

    mockFlashOpCountReset();
    mockFlashFailAfter((int32_t)n);
    bootApplySlot(1);                 // n 번째 연산 직후 '전원 차단'
    mockFlashFailAfter(-1);

    if (!invariantHolds())  { bad_invariant++; if (bad_invariant == 1) printf("    n=%u 불변식 깨짐\n", n); }
    if (!noHalfCommitted()) { bad_half++;      if (bad_half == 1)      printf("    n=%u 반쯤 커밋된 이미지 발견\n", n); }

    // 복구 : 다음 부팅에서 유효한 슬롯을 다시 적용하면 정상이 되어야 한다.
    if (bootVerifyFirm() != OK)
    {
      int8_t slot = bootGetPendingSlot();
      if (slot < 0) slot = bootGetRollbackSlot();
      if (slot < 0)
      {
        for (uint8_t i = 0; i < FLASH_SLOT_MAX; i++)
          if (bootVerifySlot(bootGetSlotAddr(i)) == OK) { slot = (int8_t)i; break; }
      }
      if (slot < 0 || bootApplySlot((uint8_t)slot) != OK || bootVerifyFirm() != OK)
      {
        no_recover++;
        if (no_recover == 1) printf("    n=%u 복구 실패\n", n);
      }
    }
  }

  CHECK(bad_invariant == 0, "부팅 불가 상태가 %d 지점에서 발생", bad_invariant);
  CHECK(bad_half == 0,      "반쯤 커밋된 이미지가 %d 지점에서 발생", bad_half);
  CHECK(no_recover == 0,    "복구 불가가 %d 지점에서 발생", no_recover);
}


//-- 부트 로그 기록 중 전원 손실
//
static void testLogPowerLoss(void)
{
  uint32_t total_ops;
  int bad = 0;

  mockFlashReset();
  bootLogClear();
  mockFlashOpCountReset();
  bootLogWrite(BOOT_EVT_UPDATE, 0, 0x1111, 0x2222, 0);
  total_ops = mockFlashOpCount();

  for (uint32_t n = 0; n < total_ops; n++)
  {
    mockFlashReset();
    bootLogClear();
    bootLogWrite(BOOT_EVT_UPDATE, 0, 0xAAAA, 0xBBBB, 0);   // 온전한 레코드 1개

    mockFlashOpCountReset();
    mockFlashFailAfter((int32_t)n);
    bootLogWrite(BOOT_EVT_ROLLBACK, 1, 0xCCCC, 0xDDDD, 0); // 중단
    mockFlashFailAfter(-1);

    // 앞의 온전한 레코드는 반드시 남아 있어야 한다.
    boot_log_t log;
    if (!bootLogRead(0, &log) || log.from_crc != 0xAAAA)
    {
      bad++;
      if (bad == 1) printf("    n=%u 기존 레코드 손상\n", n);
    }

    // 그리고 다음 기록이 정상 동작해야 한다.
    bootLogWrite(BOOT_EVT_UPDATE, 0, 0xEEEE, 0xFFFF, 0);
    bool found = false;
    for (uint16_t i = 0; i < bootLogGetCount(); i++)
      if (bootLogRead(i, &log) && log.from_crc == 0xEEEE) found = true;
    if (!found)
    {
      bad++;
      if (bad <= 2) printf("    n=%u 중단 후 새 레코드 기록 실패\n", n);
    }
  }

  CHECK(bad == 0, "로그 전원 손실 문제 %d건", bad);
}


void testPowerLoss(void)
{
  printf("[test_powerloss]\n");
  testApplyPowerLoss();
  testLogPowerLoss();

  CHECK(mockFlashViolations() == 0, "플래시 제약 위반 %u : %s",
        mockFlashViolations(), mockFlashLastViolation());
}
