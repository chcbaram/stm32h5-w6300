#include "ap.h"


static void bootUp(void);
static void updateLed(void);



void apInit(void)
{
  bootUp();

#ifdef _USE_HW_USB
  usbInit();
#endif

  bootInit();
#ifdef _USE_HW_USB
  uf2Init();
#endif
  moduleInit();
}

void apMain(void)
{
  while(1)
  {
    moduleUpdate();
  }
}

//-- 부트로더 진입 판정. moduleInit() 보다 먼저 실행되며,
//   앱으로 점프하는 경우 USB 열거를 아예 시작하지 않는다.
//
void bootUp(void)
{
  bool     run_fw = true;
  uint32_t boot_mode;
  uint16_t err_code;


  boot_mode = resetGetBootMode();

  // (1) 앱이 resetToBoot() 으로 요청한 경우
  if (boot_mode & (1<<MODE_BIT_BOOT))
  {
    logPrintf("     MODE_BIT_BOOT\n");
    run_fw = false;
  }

  // (2) 리셋 버튼 더블클릭
  if (resetGetCount() >= HW_RESET_DBLCLK_CNT)
  {
    logPrintf("     RESET DOUBLE CLICK\n");
    run_fw = false;
  }

  if (run_fw)
  {
    err_code = bootJumpFirm();      // 성공하면 돌아오지 않는다
    logPrintf("[E_] bootJumpFirm() err 0x%04X\n", err_code);
  }

  logPrintf("\n");
  logPrintf("Boot Mode..\n");
}

void updateLed(void)
{
  static uint32_t pre_time = 0;

  if (millis() - pre_time >= 100)
  {
    pre_time = millis();
    ledToggle(_DEF_LED1);
  }
}

void update(void const *arg)
{
  UNUSED(arg);

#ifdef _USE_HW_USB
  usbUpdate();
  uf2Update();
#endif
  updateLed();
}

void cliLoopIdle(void)
{
  // delay() 안에서 호출된다. moduleUpdate() 가 usbUpdate() 를 포함하므로
  // 블로킹 구간에서도 USB 가 계속 돈다. cliMain() 재진입만 막는다.
  cliMgrEnable(false);
  moduleUpdate();
  cliMgrEnable(true);
}


MODULE_DEF(ap)
{
  .name     = "ap",
  .priority = MODULE_PRI_LOW,
  .update   = update,
};
