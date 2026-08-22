#include "reset.h"
#include "rtc.h"
#include "cli.h"
#include "led.h"

#ifdef _USE_HW_RESET


#if CLI_USE(HW_RESET)
static void cliReset(cli_args_t *args);
#endif

static uint32_t resetCntLoad(void);
static void     resetCntSave(uint32_t cnt);


static bool     is_init     = false;
static uint32_t reset_bits  = 0;
static uint32_t boot_mode   = 0;
static uint32_t reset_count = 0;


static const char *reset_bit_str[RESET_BIT_MAX] =
  {
    "RESET_BIT_POWER",
    "RESET_BIT_PIN",
    "RESET_BIT_WDG",
    "RESET_BIT_SOFT",
    "RESET_BIT_ETC",
  };

static const char *mode_bit_str[MODE_BIT_MAX] =
  {
    "MODE_BIT_BOOT",
    "MODE_BIT_UPDATE",
  };



bool resetInit(void)
{
  bool ret;


#if defined(HW_RESET_BOOT) && HW_RESET_BOOT > 0
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != RESET)
  {
   reset_bits |= (1<<RESET_BIT_PIN);
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != RESET)
  {
   reset_bits |= (1<<RESET_BIT_POWER);
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET)
  {
   reset_bits |= (1<<RESET_BIT_WDG);
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != RESET)
  {
   reset_bits |= (1<<RESET_BIT_WDG);
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET)
  {
   reset_bits |= (1<<RESET_BIT_SOFT);
  }

  __HAL_RCC_CLEAR_RESET_FLAGS();

  rtcSetReg(HW_RTC_RESET_BITS, reset_bits);
#else
  rtcGetReg(HW_RTC_RESET_BITS, &reset_bits);
#endif

  rtcGetReg(HW_RTC_BOOT_MODE, &boot_mode);
  rtcSetReg(HW_RTC_BOOT_MODE, 0);


#if defined(HW_RESET_BOOT) && HW_RESET_BOOT > 0
  //-- 리셋 버튼 더블클릭 감지
  //
  //   POWER(BOR) 를 가장 먼저 검사해야 한다. STM32H5 는 전원 인가 시
  //   BORRSTF 와 PINRSTF 가 동시에 세트되므로, 순서를 뒤집으면
  //   "전원 껐다 켜기 2회" 로도 부트로더에 들어가 버린다.
  //
  {
    uint32_t cnt = resetCntLoad();

    //   또 하나, STM32H5 는 소프트 리셋(NVIC_SystemReset)에서도 내부 리셋이
    //   NRST 핀으로 전파되어 PINRSTF 가 함께 세트된다(실기 확인). 따라서
    //   SOFT/WDG 를 PIN 보다 먼저 걸러야 한다. 그렇지 않으면 resetToBoot() 이나
    //   폴트 리셋마다 더블클릭 카운트가 올라가 오진입한다.
    //
    if (reset_bits & (1<<RESET_BIT_POWER))
    {
      cnt = 0;                    // 전원 인가(BOR)는 항상 새 시작
    }
    else if (reset_bits & ((1<<RESET_BIT_SOFT) | (1<<RESET_BIT_WDG)))
    {
      cnt = 0;                    // 소프트/워치독 리셋은 집계하지 않는다
    }
    else if (reset_bits & (1<<RESET_BIT_PIN))
    {
      cnt++;                      // 순수 NRST 버튼만 집계
    }
    else
    {
      cnt = 0;
    }

    if (cnt > HW_RESET_DBLCLK_CNT)
      cnt = HW_RESET_DBLCLK_CNT;

    reset_count = cnt;

    // 두 번째 클릭을 받는 창. NRST 를 실제로 누른 경우에만 지연이 생기고
    // 전원 인가 부팅(POR)에서는 지연이 0 이다.
    //
    if (cnt == 1)
    {
      resetCntSave(cnt);
      ledOn(_DEF_LED1);
      HAL_Delay(HW_RESET_DBLCLK_MS);   // delay() 는 cliLoopIdle() 을 부르므로 쓰지 않는다
      ledOff(_DEF_LED1);
    }
    resetCntSave(0);
  }
#endif


  logPrintf("[OK] resetInit()\n");
  for (int i=0; i<RESET_BIT_MAX; i++)
  {
    if (reset_bits & (1<<i))
    {
      logPrintf("     %s\n", reset_bit_str[i]);
    }
  }
  for (int i=0; i<MODE_BIT_MAX; i++)
  {
    if (boot_mode & (1<<i))
    {
      logPrintf("     %s\n", mode_bit_str[i]);
    }
  }
  logPrintf("     reset_count : %d\n", (int)reset_count);

  is_init = true;
#if CLI_USE(HW_RESET)
  cliAdd("reset", cliReset);
#endif

  ret = is_init;
  return ret;
}

//-- 카운트 저장/로드를 분리해 둔다.
//   RTC/LSE 에 문제가 생기면 .noinit SRAM 방식으로 즉시 바꿔 끼울 수 있다.
//
uint32_t resetCntLoad(void)
{
  uint32_t reg = 0;

  rtcGetReg(HW_RTC_RESET_CNT, &reg);

  // VBAT 이 없는 보드는 전원이 끊기면 백업 도메인이 날아가 부정값이 된다.
  // 매직으로 유효성을 판정한다.
  //
  if ((reg & 0xFFFF0000UL) != HW_RESET_CNT_MAGIC)
    return 0;

  return reg & HW_RESET_CNT_MASK;
}

void resetCntSave(uint32_t cnt)
{
  rtcSetReg(HW_RTC_RESET_CNT, HW_RESET_CNT_MAGIC | (cnt & HW_RESET_CNT_MASK));
}

void resetLog(void)
{
}

void resetToBoot(void)
{
  resetSetBootMode(1<<MODE_BIT_BOOT);
  resetToReset();
}

void resetToUpdate(void)
{
  resetSetBootMode(1<<MODE_BIT_UPDATE);
  resetToReset();
}

void resetToReset(void)
{
  HAL_NVIC_SystemReset();
}

uint32_t resetGetBits(void)
{
  return reset_bits;
}

void resetSetBits(uint32_t data)
{
  reset_bits = data;
}

void resetSetBootMode(uint32_t data)
{
  boot_mode = data;
  rtcSetReg(HW_RTC_BOOT_MODE, data);
}

uint32_t resetGetBootMode(void)
{
  return boot_mode;
}

uint32_t resetGetCount(void)
{
  return reset_count;
}


//-- 부팅 확인 카운터
//
//   부트로더가 앱으로 점프하기 직전에 증가시키고, 앱이 일정 시간 정상 동작한 뒤
//   resetConfirmBoot() 으로 0 으로 되돌린다. 연속 미확인이 HW_BOOT_TRY_MAX 에
//   도달하면 이전 이미지로 롤백한다.
//
uint32_t resetGetBootTry(void)
{
  uint32_t reg = 0;

  rtcGetReg(HW_RTC_BOOT_TRY, &reg);
  if ((reg & 0xFFFF0000UL) != HW_RESET_CNT_MAGIC)
    return 0;

  return reg & HW_RESET_CNT_MASK;
}

void resetSetBootTry(uint32_t cnt)
{
  rtcSetReg(HW_RTC_BOOT_TRY, HW_RESET_CNT_MAGIC | (cnt & HW_RESET_CNT_MASK));
}

//-- 폴트 리셋 카운터
//
//   fault.c 의 faultReset() 이 NVIC_SystemReset() 직전에 증가시킨다.
//   .noinit(SRAM) 은 전원이 끊기면 사라지므로 백업 레지스터에 둔다.
//
uint32_t resetGetFaultCount(void)
{
  uint32_t reg = 0;

  rtcGetReg(HW_RTC_FAULT_CNT, &reg);
  if ((reg & 0xFFFF0000UL) != HW_RESET_CNT_MAGIC)
    return 0;

  return reg & HW_RESET_CNT_MASK;
}

void resetIncFaultCount(void)
{
  uint32_t cnt = resetGetFaultCount();

  if (cnt < HW_RESET_CNT_MASK)
    cnt++;

  rtcSetReg(HW_RTC_FAULT_CNT, HW_RESET_CNT_MAGIC | cnt);
}

void resetConfirmBoot(void)
{
  resetSetBootTry(0);
  rtcSetReg(HW_RTC_FAULT_CNT, HW_RESET_CNT_MAGIC | 0);
}

bool resetGetEccAddr(uint32_t *p_addr)
{
  uint32_t reg = 0;

  rtcGetReg(HW_RTC_ECC_ADDR, &reg);
  if ((reg & 0xFF000000UL) != HW_ECC_MAGIC)
    return false;

  // [20] bank(0=bank1, 1=bank2), [15:0] 뱅크 내 쿼드워드 오프셋 인덱스
  {
    uint32_t bank   = (reg >> 20) & 0x1;
    uint32_t offset = (reg & 0xFFFFUL) * 16;

    *p_addr = 0x08000000UL + bank * 0x00100000UL + offset;
  }
  return true;
}

void resetClearEccAddr(void)
{
  rtcSetReg(HW_RTC_ECC_ADDR, 0);
}


#if CLI_USE(HW_RESET)
void cliReset(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    uint32_t ecc_addr;

    cliPrintf("Reset Bits  : 0x%X\n", reset_bits);
    for (int i=0; i<RESET_BIT_MAX; i++)
    {
      if (reset_bits & (1<<i))
      {
        cliPrintf("      %s\n", reset_bit_str[i]);
      }
    }
    cliPrintf("Boot Mode   : 0x%X\n", boot_mode);
    for (int i=0; i<MODE_BIT_MAX; i++)
    {
      if (boot_mode & (1<<i))
      {
        cliPrintf("      %s\n", mode_bit_str[i]);
      }
    }
    cliPrintf("reset count : %d\n", (int)reset_count);
    cliPrintf("boot try    : %d\n", (int)resetGetBootTry());
    cliPrintf("fault count : %d\n", (int)resetGetFaultCount());
    if (resetGetEccAddr(&ecc_addr))
      cliPrintf("ecc addr    : 0x%08X\n", ecc_addr);
    else
      cliPrintf("ecc addr    : -\n");
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "boot"))
  {
    resetToBoot();
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "update"))
  {
    resetToUpdate();
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "reset"))
  {
    resetToReset();
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "fault"))
  {
    if (args->isStr(1, "inc"))
    {
      resetIncFaultCount();
      cliPrintf("fault count : %d\n", (int)resetGetFaultCount());
      ret = true;
    }
    if (args->isStr(1, "clear"))
    {
      resetConfirmBoot();
      cliPrintf("cleared\n");
      ret = true;
    }
  }

  if (ret == false)
  {
    cliPrintf("reset info\n");
    cliPrintf("reset boot\n");
    cliPrintf("reset update\n");
    cliPrintf("reset reset\n");
    cliPrintf("reset fault inc|clear\n");
  }
}
#endif


#endif
