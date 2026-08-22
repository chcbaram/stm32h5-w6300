#ifndef TEST_COMMON_H_
#define TEST_COMMON_H_

#include <stdio.h>
#include <string.h>
#include "boot/boot.h"
#include "mock_flash.h"

extern int g_pass, g_fail;

#define CHECK(cond, ...)                                                     \
  do {                                                                       \
    if (cond) { g_pass++; }                                                  \
    else {                                                                   \
      g_fail++;                                                              \
      printf("  FAIL %s:%d  ", __FILE__, __LINE__);                          \
      printf(__VA_ARGS__); printf("\n");                                     \
    }                                                                        \
  } while (0)

void mockLogEnable(bool enable);
void mockResetClear(void);

// 슬롯/FIRM 에 유효한 이미지를 만들어 넣는다. 내용은 seed 로 결정한다.
void testMakeImage(uint32_t base, uint32_t size, uint32_t seq, uint8_t seed);
uint16_t testImageCrc(uint32_t size, uint8_t seed);

#endif
