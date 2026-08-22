#ifndef MOCK_FLASH_H_
#define MOCK_FLASH_H_

#include <stdint.h>
#include <stdbool.h>

#define MOCK_FLASH_BASE     0x08000000UL
#define MOCK_FLASH_SIZE     (2u*1024u*1024u)


void     mockFlashReset(void);                 // 전체를 0xFF 로
uint8_t *mockFlashPtr(uint32_t addr);

// n 번째 플래시 '연산'(erase/program) 직후부터 모든 연산을 실패시킨다.
// 전원 손실 시뮬레이션. -1 이면 해제.
void     mockFlashFailAfter(int32_t n);
uint32_t mockFlashOpCount(void);
void     mockFlashOpCountReset(void);

// 제약 위반이 몇 번 있었는지 (0 이어야 정상)
uint32_t mockFlashViolations(void);
const char *mockFlashLastViolation(void);

#endif
