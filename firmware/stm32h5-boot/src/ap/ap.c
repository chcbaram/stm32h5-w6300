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
#ifdef _USE_HW_CMD
  cmdTaskInit();
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
  bool     apply_done = false;
  uint32_t boot_mode;
  uint32_t ecc_addr;
  uint16_t err_code;
  int8_t   slot;


  boot_mode = resetGetBootMode();

  //-- (0) 지난 부팅에서 ECC 2비트 오류가 났다면 그 영역을 정리한다.
  //       NMI_Handler 는 위치만 남기고 리셋하므로 실제 정리는 여기서 한다.
  //
  if (resetGetEccAddr(&ecc_addr))
  {
    logPrintf("[!!] ECC error at 0x%08X\n", (unsigned int)ecc_addr);

    for (uint8_t i = 0; i < FLASH_SLOT_MAX; i++)
    {
      uint32_t base = bootGetSlotAddr(i);

      if (ecc_addr >= base && ecc_addr < (base + FLASH_SIZE_SLOT))
      {
        logPrintf("     clean slot%d\n", i);
        bootInvalidateSlot(i);
        break;
      }
    }
    if (ecc_addr >= FLASH_ADDR_BOOT_LOG &&
        ecc_addr < (FLASH_ADDR_BOOT_LOG + FLASH_SIZE_BOOT_LOG))
    {
      logPrintf("     clean boot log\n");
      bootLogClear();
    }

    resetClearEccAddr();
    bootLogWrite(BOOT_EVT_ECC_CLEAN, -1, ecc_addr, 0, 0);
  }

  //-- (1) 앱이 resetToBoot() 으로 요청한 경우
  //
  if (boot_mode & (1<<MODE_BIT_BOOT))
  {
    logPrintf("     MODE_BIT_BOOT\n");
    run_fw = false;
  }

  //-- (2) 리셋 버튼 더블클릭
  //
  if (resetGetCount() >= HW_RESET_DBLCLK_CNT)
  {
    logPrintf("     RESET DOUBLE CLICK\n");
    run_fw = false;
  }

  //-- (3) 폴트 반복 -> 자동 복구 / (4) 부팅 미확인 반복 -> 롤백
  //
  //   둘 다 같은 경로(롤백 슬롯 적용 + 실패 슬롯 무효화)를 쓰고 로그 이벤트만 다르다.
  //   롤백한 이미지마저 죽으면 카운트가 다시 차는데, 그때는 롤백 슬롯이 없어(-1)
  //   UF2 모드로 빠지므로 무한 롤백 루프가 생기지 않는다.
  //
  if (run_fw)
  {
    uint32_t   fault_cnt = resetGetFaultCount();
    uint32_t   try_cnt   = resetGetBootTry();
    boot_evt_t evt       = BOOT_EVT_ROLLBACK;
    bool       need_rollback = false;

    if (fault_cnt >= HW_BOOT_FAULT_MAX)
    {
      logPrintf("[!!] FAULT x%d -> recover\n", (int)fault_cnt);
      evt = BOOT_EVT_FAULT_RECOVER;
      need_rollback = true;
    }
    else if (try_cnt >= HW_BOOT_TRY_MAX)
    {
      logPrintf("[!!] BOOT TRY x%d -> rollback\n", (int)try_cnt);
      evt = BOOT_EVT_ROLLBACK;
      need_rollback = true;
    }

    if (need_rollback)
    {
      boot_slot_info_t firm;

      bootGetFirmInfo(&firm);
      slot = bootGetRollbackSlot();

      if (slot >= 0)
      {
        boot_slot_info_t info;

        bootGetSlotInfo((uint8_t)slot, &info);
        err_code = bootApplySlot((uint8_t)slot);

        if (err_code == OK)
        {
          logPrintf("[OK] rollback -> slot%d\n", slot);
          bootLogWrite(evt, slot, firm.fw_crc, info.fw_crc, faultGetPc());

          // 실패한 이미지가 담긴 슬롯은 버린다.
          for (uint8_t i = 0; i < FLASH_SLOT_MAX; i++)
          {
            boot_slot_info_t bad;

            bootGetSlotInfo(i, &bad);
            if (bad.valid && bad.fw_size == firm.fw_size && bad.fw_crc == firm.fw_crc)
            {
              bootInvalidateSlot(i);
              break;
            }
          }
          apply_done = true;
        }
        else
        {
          logPrintf("[E_] rollback err 0x%04X\n", err_code);
          bootLogWrite(BOOT_EVT_VERIFY_FAIL, slot, firm.fw_crc, 0, faultGetPc());
          run_fw = false;
        }
      }
      else
      {
        // 되돌아갈 곳이 없다. 엉뚱한 버전으로 되돌리는 것보다 UF2 모드가 낫다.
        logPrintf("[E_] no rollback slot -> UF2 mode\n");
        bootLogWrite(BOOT_EVT_VERIFY_FAIL, -1, firm.fw_crc, 0, faultGetPc());
        run_fw = false;
      }

      resetConfirmBoot();
    }
  }

  //-- (5) 앱이 요청한 업데이트 적용
  //
  if (run_fw && !apply_done && (boot_mode & (1<<MODE_BIT_UPDATE)))
  {
    slot = bootGetPendingSlot();
    if (slot >= 0)
    {
      boot_slot_info_t firm, info;

      bootGetFirmInfo(&firm);
      bootGetSlotInfo((uint8_t)slot, &info);

      err_code = bootApplySlot((uint8_t)slot);
      if (err_code == OK)
        bootLogWrite(BOOT_EVT_UPDATE, slot, firm.fw_crc, info.fw_crc, 0);
      else
        bootLogWrite(BOOT_EVT_VERIFY_FAIL, slot, firm.fw_crc, 0, 0);

      logPrintf("[%s] MODE_BIT_UPDATE slot%d err 0x%04X\n",
                err_code == OK ? "OK" : "E_", slot, err_code);
    }
  }

  //-- (6) 점프
  //
  if (run_fw)
  {
    // 점프 직전에 부팅 시도 횟수를 올린다. 앱이 일정 시간 정상 동작하면
    // resetConfirmBoot() 으로 0 이 된다.
    resetSetBootTry(resetGetBootTry() + 1);

    err_code = bootJumpFirm();      // 성공하면 돌아오지 않는다
    logPrintf("[E_] bootJumpFirm() err 0x%04X\n", err_code);

    // 점프 실패. 유효한 슬롯이 있으면 한 번만 재적용해 본다.
    slot = bootGetPendingSlot();
    if (slot < 0)
      slot = bootGetRollbackSlot();

    if (slot >= 0 && bootApplySlot((uint8_t)slot) == OK)
    {
      bootLogWrite(BOOT_EVT_UPDATE, slot, 0, 0, 0);
      bootJumpFirm();
    }
    bootLogWrite(BOOT_EVT_VERIFY_FAIL, slot, 0, 0, 0);
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
#ifdef _USE_HW_CMD
  cmdTaskUpdate();
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
