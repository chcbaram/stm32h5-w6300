#include "bsp.h"
#include "cli.h"
#include "led.h"
#include "reset.h"
#include "mock_flash.h"
#include <stdarg.h>


//-- 호스트 테스트용 최소 환경.
//
static bool     s_log_enable = false;
static uint32_t s_backup[16];
static uint32_t s_millis = 0;


void mockLogEnable(bool enable) { s_log_enable = enable; }

void logPrintf(const char *fmt, ...)
{
  va_list args;

  if (!s_log_enable)
    return;

  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

void cliPrintf(const char *fmt, ...) { (void)fmt; }
bool cliAdd(const char *c, void (*f)(cli_args_t *)) { (void)c; (void)f; return true; }

void ledToggle(uint8_t ch) { (void)ch; }
void ledOn(uint8_t ch)     { (void)ch; }
void ledOff(uint8_t ch)    { (void)ch; }

void     delay(uint32_t ms) { s_millis += ms; }
uint32_t millis(void)       { return s_millis; }
bool     bspDeInit(void)    { return true; }

//-- reset.c 대신 백업 레지스터만 흉내낸다.
void mockResetClear(void) { memset(s_backup, 0, sizeof(s_backup)); }

uint32_t resetGetBits(void)        { return s_backup[4]; }
void     resetSetBits(uint32_t d)  { s_backup[4] = d; }
uint32_t resetGetBootMode(void)    { return s_backup[3]; }
void     resetSetBootMode(uint32_t d) { s_backup[3] = d; }
uint32_t resetGetCount(void)       { return 0; }
uint32_t resetGetBootTry(void)     { return s_backup[6]; }
void     resetSetBootTry(uint32_t c) { s_backup[6] = c; }
uint32_t resetGetFaultCount(void)  { return s_backup[7]; }
void     resetIncFaultCount(void)  { s_backup[7]++; }
void     resetConfirmBoot(void)    { s_backup[6] = 0; s_backup[7] = 0; }
bool     resetGetEccAddr(uint32_t *p) { (void)p; return false; }
void     resetClearEccAddr(void)   { }
void     resetToBoot(void)   { }
void     resetToUpdate(void) { }
void     resetToReset(void)  { }
void     resetLog(void)      { }
bool     resetInit(void)     { return true; }
