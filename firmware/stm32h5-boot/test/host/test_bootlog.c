#include "test_common.h"


void testBootLog(void)
{
  boot_log_t log;

  printf("[test_bootlog]\n");

  mockFlashReset();
  CHECK(bootLogClear(), "로그 초기화");
  CHECK(bootLogGetCount() == 0, "초기 레코드 0");

  //-- 기본 기록/조회
  CHECK(bootLogWrite(BOOT_EVT_UPDATE, 0, 0x1111, 0x2222, 0), "기록 1");
  CHECK(bootLogWrite(BOOT_EVT_ROLLBACK, 1, 0x2222, 0x1111, 0x08001234), "기록 2");
  CHECK(bootLogGetCount() == 2, "레코드 2개 (%u)", bootLogGetCount());

  CHECK(bootLogRead(0, &log) && log.event == BOOT_EVT_UPDATE && log.from_crc == 0x1111,
        "0번 레코드 내용");
  CHECK(bootLogRead(1, &log) && log.event == BOOT_EVT_ROLLBACK && log.fault_pc == 0x08001234,
        "1번 레코드 폴트 PC");
  CHECK(log.seq == 2, "seq 는 단조 증가 (%u)", log.seq);

  //-- 섹터를 가득 채워 핑퐁 전환을 확인한다
  mockFlashReset();
  bootLogClear();
  for (uint32_t i = 0; i < BOOT_LOG_REC_MAX; i++)
    bootLogWrite(BOOT_EVT_UPDATE, 0, i, i, 0);

  CHECK(bootLogGetCount() == BOOT_LOG_REC_MAX, "섹터 하나 가득 (%u)", bootLogGetCount());

  // 한 개 더 쓰면 반대편 섹터로 넘어간다. 이전 세대는 보존된다.
  CHECK(bootLogWrite(BOOT_EVT_ROLLBACK, 1, 0xDEAD, 0xBEEF, 0), "핑퐁 전환 기록");
  CHECK(bootLogGetCount() == BOOT_LOG_REC_MAX + 1,
        "이전 세대가 보존되어 총 %u (기대 %u)", bootLogGetCount(), BOOT_LOG_REC_MAX + 1);

  // 가장 마지막 레코드가 방금 쓴 것이어야 한다
  CHECK(bootLogRead(BOOT_LOG_REC_MAX, &log) && log.from_crc == 0xDEAD,
        "마지막 레코드가 새 세대의 첫 기록");

  // 가장 앞 레코드는 이전 세대의 첫 기록
  CHECK(bootLogRead(0, &log) && log.from_crc == 0, "앞 레코드는 이전 세대");

  CHECK(mockFlashViolations() == 0, "플래시 제약 위반 %u : %s",
        mockFlashViolations(), mockFlashLastViolation());
}
