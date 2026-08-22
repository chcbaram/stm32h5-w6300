#include "ap.h"
#include "iperf.h"





void apInit(void)
{
  bootInit();
  moduleInit();
}

void apMain(void)
{
  while(1)
  {
    moduleUpdate();
  }
}

void updateLED(void)
{
  static uint32_t pre_time = 0;
  
  
  if (millis() - pre_time >= 500)
  {
    pre_time = millis();
    ledToggle(_DEF_LED1);
  }
}

//-- 부팅 성공 확정.
//
//   부트로더는 앱으로 점프하기 직전에 boot_try 카운터를 올린다. 앱이 여기서
//   0 으로 되돌리지 않으면 부트로더는 "앱이 부팅에 실패했다" 고 보고 연속
//   HW_BOOT_TRY_MAX 회 뒤에 이전 이미지로 롤백한다.
//
//   즉시 확정하지 않고 HW_BOOT_CONFIRM_MS 만큼 정상 동작한 뒤에 확정한다.
//   그래야 "부팅 몇 초 뒤에 항상 죽는" 펌웨어도 걸러진다.
//
void updateBootConfirm(void)
{
  static bool is_confirmed = false;

  if (is_confirmed)
    return;

  if (millis() >= HW_BOOT_CONFIRM_MS)
  {
    resetConfirmBoot();
    is_confirmed = true;
    logPrintf("[OK] resetConfirmBoot()\n");
  }
}

void update(void const *arg)
{
  eventUpdate();
  wiznetUpdate();  
  updateLED();    
  updateBootConfirm();
}

void cliLoopIdle(void)
{
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