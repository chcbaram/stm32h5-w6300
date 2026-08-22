#include "boot_log.h"


//-- 부트 이벤트 로그
//
//   8KB 섹터 2개를 컴팩션 없이 핑퐁으로 쓴다.
//     - 섹터 A 가 차면 섹터 B 를 지우고 B 에 append (A 는 이전 세대로 보존)
//     - 읽을 때는 각 섹터 첫 레코드의 seq 로 세대 순서를 판별
//   항상 최소 한 섹터가 온전하므로, 소거 도중 전원이 끊겨도 이력을 통째로
//   잃지 않는다. 단일 섹터 순환은 이 두 가지가 모두 위험하다.
//
static bool     is_init  = false;
static uint8_t  cur_sect = 0;        // 현재 append 중인 섹터
static uint16_t cur_idx  = 0;        // 다음에 쓸 레코드 인덱스
static uint32_t next_seq = 1;

static uint32_t bootLogGetTime(void);
static uint32_t bootLogSectAddr(uint8_t sect);
static bool     bootLogReadRec(uint8_t sect, uint16_t idx, boot_log_t *p_log);
static bool     bootLogRecIsBlank(uint8_t sect, uint16_t idx);
static uint16_t bootLogScanSect(uint8_t sect, uint32_t *p_first_seq,
                                uint32_t *p_last_seq, uint16_t *p_next_idx);
static bool     bootLogReadNth(uint8_t sect, uint16_t n, boot_log_t *p_log);



bool bootLogInit(void)
{
  uint32_t first[BOOT_LOG_SECTOR_MAX] = {0};
  uint32_t last[BOOT_LOG_SECTOR_MAX]  = {0};
  uint16_t cnt[BOOT_LOG_SECTOR_MAX]   = {0};

  uint16_t nxt[BOOT_LOG_SECTOR_MAX] = {0};

  for (uint8_t s = 0; s < BOOT_LOG_SECTOR_MAX; s++)
    cnt[s] = bootLogScanSect(s, &first[s], &last[s], &nxt[s]);

  // seq 가 가장 큰 섹터가 현재 세대다.
  cur_sect = 0;
  for (uint8_t s = 1; s < BOOT_LOG_SECTOR_MAX; s++)
  {
    if (cnt[s] > 0 && (cnt[cur_sect] == 0 || last[s] > last[cur_sect]))
      cur_sect = s;
  }

  cur_idx  = nxt[cur_sect];
  next_seq = (cnt[cur_sect] > 0) ? last[cur_sect] + 1 : 1;

  is_init = true;
  return true;
}

//-- 레코드 기록.
//
//   커밋 규칙을 따른다.
//     (2) 둘째 쿼드워드(0x10~0x1F) 먼저
//     (1) magic 이 든 첫 쿼드워드를 마지막
//   중간에 끊기면 magic 이 없어 빈 자리로 보이고, 그 자리는 이미 프로그램되어
//   있으므로 다음 기록은 다음 자리로 건너뛴다.
//
bool bootLogWrite(boot_evt_t evt, int8_t slot, uint32_t from_crc,
                  uint32_t to_crc, uint32_t fault_pc)
{
  boot_log_t log;
  uint8_t    buf[16] __attribute__((aligned(16)));
  uint32_t   addr;

  if (is_init != true)
    bootLogInit();

  // 섹터가 찼으면 반대편 섹터를 지우고 그쪽으로 넘어간다.
  if (cur_idx >= BOOT_LOG_REC_MAX)
  {
    uint8_t next = (cur_sect + 1) % BOOT_LOG_SECTOR_MAX;

    if (flashErase(bootLogSectAddr(next), FLASH_SECTOR_SIZE) != true)
      return false;

    cur_sect = next;
    cur_idx  = 0;
  }

  // 부분 기록이 남은 자리는 건너뛴다 (쿼드워드 재기록 불가).
  while (cur_idx < BOOT_LOG_REC_MAX && bootLogRecIsBlank(cur_sect, cur_idx) != true)
    cur_idx++;

  if (cur_idx >= BOOT_LOG_REC_MAX)
    return false;

  addr = bootLogSectAddr(cur_sect) + (uint32_t)cur_idx * BOOT_LOG_REC_SIZE;

  memset(&log, 0xFF, sizeof(log));
  log.magic      = BOOT_LOG_MAGIC;
  log.seq        = next_seq;
  log.event      = (uint8_t)evt;
  log.slot       = (slot < 0) ? 0xFF : (uint8_t)slot;
  log.reset_bits = (uint16_t)resetGetBits();
  log.timestamp  = bootLogGetTime();
  log.from_crc   = from_crc;
  log.to_crc     = to_crc;
  log.fault_pc   = fault_pc;
  log.crc        = utilCalcCRC(0, (uint8_t *)&log, 28);

  // (2) 둘째 쿼드워드
  memcpy(buf, ((uint8_t *)&log) + 16, 16);
  if (flashWrite(addr + 16, buf, 16) != true)
    return false;

  // (1) 커밋 : magic 이 든 첫 쿼드워드
  memcpy(buf, &log, 16);
  if (flashWrite(addr, buf, 16) != true)
    return false;

  cur_idx++;
  next_seq++;
  return true;
}

//-- 기록 시각.
//
//   보드에 코인셀이 없어 전원을 뽑으면 RTC 가 초기화된다. rtcGetEpoch() 은 그때
//   0 을 돌려주고, 조회하는 쪽(CLI/웹)은 0 을 "시각 없음" 으로 표시한다.
//
//   RTC 는 리셋에는 살아남으므로, 앱이 SNTP 나 호스트 동기화로 한 번 맞춰두면
//   그 뒤의 롤백/폴트 복구 기록에는 제대로 된 시각이 남는다. 그게 정작 알고
//   싶은 경우다.
//
static uint32_t bootLogGetTime(void)
{
#ifdef _USE_HW_RTC
  return rtcGetEpoch();
#else
  return 0;
#endif
}

//-- 조회는 두 섹터를 seq 순으로 이어 붙여 본다.
//
uint16_t bootLogGetCount(void)
{
  uint32_t f, l;
  uint16_t total = 0;

  for (uint8_t s = 0; s < BOOT_LOG_SECTOR_MAX; s++)
    total += bootLogScanSect(s, &f, &l, NULL);

  return total;
}

bool bootLogRead(uint16_t idx, boot_log_t *p_log)
{
  uint32_t f[BOOT_LOG_SECTOR_MAX], l[BOOT_LOG_SECTOR_MAX];
  uint16_t c[BOOT_LOG_SECTOR_MAX];
  uint8_t  order[BOOT_LOG_SECTOR_MAX];
  uint8_t  n = 0;

  for (uint8_t s = 0; s < BOOT_LOG_SECTOR_MAX; s++)
  {
    c[s] = bootLogScanSect(s, &f[s], &l[s], NULL);
    if (c[s] > 0)
      order[n++] = s;
  }

  // seq 가 작은 섹터(이전 세대)가 앞이다.
  if (n == 2 && f[order[0]] > f[order[1]])
  {
    uint8_t t = order[0]; order[0] = order[1]; order[1] = t;
  }

  for (uint8_t i = 0; i < n; i++)
  {
    if (idx < c[order[i]])
      return bootLogReadNth(order[i], idx, p_log);
    idx -= c[order[i]];
  }
  return false;
}

bool bootLogClear(void)
{
  for (uint8_t s = 0; s < BOOT_LOG_SECTOR_MAX; s++)
  {
    if (flashErase(bootLogSectAddr(s), FLASH_SECTOR_SIZE) != true)
      return false;
  }
  cur_sect = 0;
  cur_idx  = 0;
  next_seq = 1;
  return true;
}

static uint32_t bootLogSectAddr(uint8_t sect)
{
  return FLASH_ADDR_BOOT_LOG + (uint32_t)sect * FLASH_SECTOR_SIZE;
}

static bool bootLogReadRec(uint8_t sect, uint16_t idx, boot_log_t *p_log)
{
  if (sect >= BOOT_LOG_SECTOR_MAX || idx >= BOOT_LOG_REC_MAX)
    return false;

  if (flashRead(bootLogSectAddr(sect) + (uint32_t)idx * BOOT_LOG_REC_SIZE,
                (uint8_t *)p_log, sizeof(boot_log_t)) != true)
    return false;

  return (p_log->magic == BOOT_LOG_MAGIC);
}

static bool bootLogRecIsBlank(uint8_t sect, uint16_t idx)
{
  uint8_t chk[BOOT_LOG_REC_SIZE];

  if (flashRead(bootLogSectAddr(sect) + (uint32_t)idx * BOOT_LOG_REC_SIZE,
                chk, sizeof(chk)) != true)
    return false;

  for (uint32_t i = 0; i < sizeof(chk); i++)
  {
    if (chk[i] != 0xFF)
      return false;
  }
  return true;
}

//-- 섹터 안의 유효 레코드 개수 / 첫·마지막 seq / 다음 기록 위치를 찾는다.
//
//   섹터 전체를 훑고 빈틈을 건너뛴다. 첫 무효 레코드에서 멈추면 안 된다.
//   기록 도중 전원이 끊기면 magic 이 없는 자리가 하나 생기는데, 거기서 스캔을
//   멈추면 그 뒤의 정상 레코드가 영영 보이지 않는다(호스트 전원손실 시험에서
//   실제로 잡힌 버그다).
//
static uint16_t bootLogScanSect(uint8_t sect, uint32_t *p_first_seq,
                                uint32_t *p_last_seq, uint16_t *p_next_idx)
{
  boot_log_t log;
  uint16_t   count = 0;
  uint16_t   next  = 0;

  *p_first_seq = 0;
  *p_last_seq  = 0;

  for (uint16_t i = 0; i < BOOT_LOG_REC_MAX; i++)
  {
    if (bootLogRecIsBlank(sect, i))
      continue;                    // 아직 안 쓴 자리

    next = i + 1;                  // 뭔가 쓰여 있으면 다음 기록은 이 뒤로

    if (bootLogReadRec(sect, i, &log) != true)
      continue;                    // 부분 기록 (magic 없음) -> 건너뛴다

    if (count == 0)
      *p_first_seq = log.seq;
    *p_last_seq = log.seq;
    count++;
  }

  if (p_next_idx != NULL)
    *p_next_idx = next;

  return count;
}

//-- 섹터 안에서 n 번째 '유효' 레코드를 찾는다 (빈틈 건너뜀).
//
static bool bootLogReadNth(uint8_t sect, uint16_t n, boot_log_t *p_log)
{
  uint16_t seen = 0;

  for (uint16_t i = 0; i < BOOT_LOG_REC_MAX; i++)
  {
    if (bootLogRecIsBlank(sect, i))
      continue;
    if (bootLogReadRec(sect, i, p_log) != true)
      continue;
    if (seen == n)
      return true;
    seen++;
  }
  return false;
}
