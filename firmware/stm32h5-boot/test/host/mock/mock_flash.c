#include "mock_flash.h"
#include "flash.h"
#include <string.h>
#include <stdio.h>


//-- STM32H5 내부 플래시 에뮬레이터.
//
//   이 목의 존재 이유는 "동작을 흉내내는 것" 이 아니라 **제약을 강제하는 것** 이다.
//   실기에서 확인했듯이 HAL 은 쿼드워드 재기록을 막아주지 않고 ECC 만 조용히
//   깨진다. 목이 이런 위반을 즉시 잡아주면 벽돌을 만들기 전에 발견할 수 있다.
//
#define SECTOR_SIZE     0x2000u        // 8KB
#define WRITE_SIZE      16u            // quad-word
#define BANK_SIZE       0x100000u

static uint8_t  s_flash[MOCK_FLASH_SIZE];
static int32_t  s_fail_after = -1;
static uint32_t s_op_count   = 0;
static uint32_t s_violations = 0;
static char     s_last_violation[160];


static void violation(const char *fmt, uint32_t a, uint32_t b)
{
  s_violations++;
  snprintf(s_last_violation, sizeof(s_last_violation), fmt, a, b);
}

void mockFlashReset(void)
{
  memset(s_flash, 0xFF, sizeof(s_flash));
  s_fail_after = -1;
  s_op_count   = 0;
  s_violations = 0;
  s_last_violation[0] = 0;
}

uint8_t *mockFlashPtr(uint32_t addr)
{
  return &s_flash[addr - MOCK_FLASH_BASE];
}

void     mockFlashFailAfter(int32_t n) { s_fail_after = n; }
uint32_t mockFlashOpCount(void)        { return s_op_count; }
void     mockFlashOpCountReset(void)   { s_op_count = 0; }
uint32_t mockFlashViolations(void)     { return s_violations; }
const char *mockFlashLastViolation(void) { return s_last_violation; }


static bool inRange(uint32_t addr, uint32_t len)
{
  if (len == 0) return false;
  if (addr < MOCK_FLASH_BASE) return false;
  if ((uint64_t)addr + len > (uint64_t)MOCK_FLASH_BASE + MOCK_FLASH_SIZE) return false;
  return true;
}

static bool isProtected(uint32_t addr, uint32_t len)
{
  uint32_t s = addr, e = addr + len - 1;
  uint32_t bs = FLASH_PROTECT_ADDR, be = FLASH_PROTECT_ADDR + FLASH_PROTECT_SIZE - 1;
  return !(e < bs || s > be);
}

// 전원 손실 지점에 도달했는지. true 면 이 연산은 수행되지 않는다.
static bool opFailed(void)
{
  if (s_fail_after >= 0 && (int32_t)s_op_count >= s_fail_after)
    return true;
  s_op_count++;
  return false;
}


bool flashInit(void)
{
  return true;
}

bool flashErase(uint32_t addr, uint32_t length)
{
  if (!inRange(addr, length))  return false;
  if (isProtected(addr, length))
  {
    violation("erase protected 0x%08X len %u", addr, length);
    return false;
  }

  // 섹터 정렬 확인 : 부분 소거는 하드웨어에 없다.
  uint32_t s = (addr - MOCK_FLASH_BASE) / SECTOR_SIZE;
  uint32_t e = (addr + length - 1 - MOCK_FLASH_BASE) / SECTOR_SIZE;

  if (opFailed()) return false;

  for (uint32_t i = s; i <= e; i++)
    memset(&s_flash[i * SECTOR_SIZE], 0xFF, SECTOR_SIZE);

  return true;
}

bool flashWrite(uint32_t addr, uint8_t *p_data, uint32_t length)
{
  if (length == 0) return true;
  if (!inRange(addr, length)) return false;

  if (isProtected(addr, length))
  {
    violation("write protected 0x%08X len %u", addr, length);
    return false;
  }
  if ((addr % WRITE_SIZE) != 0)
  {
    violation("unaligned write 0x%08X len %u", addr, length);
    return false;
  }

  for (uint32_t off = 0; off < length; off += WRITE_SIZE)
  {
    uint32_t n = length - off;
    uint8_t  buf[WRITE_SIZE];
    uint8_t *dst = mockFlashPtr(addr + off);

    if (n > WRITE_SIZE) n = WRITE_SIZE;

    // 이미 프로그램된 쿼드워드 재기록 금지.
    // 실기에서는 HAL 이 OK 를 반환하고 ECC 만 깨진다. 여기서는 즉시 잡는다.
    for (uint32_t i = 0; i < WRITE_SIZE; i++)
    {
      if (dst[i] != 0xFF)
      {
        violation("rewrite non-blank quad-word 0x%08X len %u", addr + off, length);
        return false;
      }
    }

    memset(buf, 0xFF, WRITE_SIZE);
    memcpy(buf, &p_data[off], n);

    if (opFailed()) return false;
    memcpy(dst, buf, WRITE_SIZE);
  }
  return true;
}

bool flashRead(uint32_t addr, uint8_t *p_data, uint32_t length)
{
  if (!inRange(addr, length)) return false;
  memcpy(p_data, mockFlashPtr(addr), length);
  return true;
}
