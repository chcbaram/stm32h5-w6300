#include "boot.h"


#if CLI_USE(HW_BOOT)
static void cliBoot(cli_args_t *args);
#endif

static bool bootReadSlotMeta(uint32_t addr, boot_slot_t *p_slot);
static bool bootIsSlotInvalidated(uint32_t addr);



bool bootInit(void)
{
  bootLogInit();

#if CLI_USE(HW_BOOT)
  cliAdd("boot", cliBoot);
#endif
  return true;
}

uint32_t bootGetSlotAddr(uint8_t index)
{
  const uint32_t addr_tbl[FLASH_SLOT_MAX] = {FLASH_ADDR_SLOT0, FLASH_ADDR_SLOT1};

  if (index >= FLASH_SLOT_MAX)
    return 0;

  return addr_tbl[index];
}

//-- 슬롯/FIRM 공용 검증.
//
//   TAG 매직을 먼저 확인하고 그 다음에 본문을 읽는 순서를 반드시 지킨다.
//   TAG 는 항상 마지막에 커밋되므로 "TAG 유효 = 본문 기록 완료" 가 보장되고,
//   리셋으로 중단된 반쯤 기록된 영역을 읽어 ECC 오류를 내는 일을 피할 수 있다.
//
uint16_t bootVerifySlot(uint32_t addr)
{
  firm_tag_t tag;
  firm_tag_t *p_tag = &tag;
  uint16_t    err_code = OK;
  uint16_t    crc = 0;
  uint8_t     rd_buf[256];

  if (flashRead(addr, (uint8_t *)p_tag, sizeof(firm_tag_t)) != true)
    return ERR_BOOT_FLASH_READ;

  do
  {
    if (p_tag->magic_number != TAG_MAGIC_NUMBER)
    {
      err_code = ERR_BOOT_TAG_MAGIC;
      break;
    }

    // 상한은 FIRM 영역에서 TAG 를 뺀 크기다.
    if (p_tag->fw_size == 0 || p_tag->fw_size > (FLASH_SIZE_FIRM - FLASH_SIZE_TAG))
    {
      err_code = ERR_BOOT_TAG_SIZE;
      break;
    }

    if (p_tag->fw_addr != FLASH_SIZE_TAG)
    {
      err_code = ERR_BOOT_TAG_SIZE;
      break;
    }

    uint32_t index  = 0;
    uint32_t length = p_tag->fw_size;

    while (index < length)
    {
      uint32_t rd_len = length - index;

      if (rd_len > sizeof(rd_buf))
        rd_len = sizeof(rd_buf);

      if (flashRead(addr + p_tag->fw_addr + index, rd_buf, rd_len) != true)
      {
        err_code = ERR_BOOT_FLASH_READ;
        break;
      }
      crc = utilCalcCRC(crc, rd_buf, rd_len);
      index += rd_len;
    }

    if (err_code == OK && p_tag->fw_crc != crc)
    {
      err_code = ERR_BOOT_FW_CRC;
    }
  } while (0);

  return err_code;
}

uint16_t bootVerifyFirm(void)
{
  return bootVerifySlot(FLASH_ADDR_FIRM);
}

bool bootReadSlotMeta(uint32_t addr, boot_slot_t *p_slot)
{
  if (flashRead(addr + BOOT_SLOT_TAG_OFFSET, (uint8_t *)p_slot, sizeof(boot_slot_t)) != true)
    return false;

  return (p_slot->magic == BOOT_SLOT_MAGIC);
}

bool bootIsSlotInvalidated(uint32_t addr)
{
  uint32_t magic = 0;

  if (flashRead(addr + BOOT_SLOT_INV_OFFSET, (uint8_t *)&magic, sizeof(magic)) != true)
    return false;

  return (magic == BOOT_SLOT_INV_MAGIC);
}

bool bootGetSlotInfo(uint8_t index, boot_slot_info_t *p_info)
{
  uint32_t    addr = bootGetSlotAddr(index);
  firm_tag_t  tag;
  boot_slot_t slot;

  if (index >= FLASH_SLOT_MAX || p_info == NULL)
    return false;

  memset(p_info, 0, sizeof(boot_slot_info_t));
  p_info->index = index;
  p_info->addr  = addr;

  if (bootIsSlotInvalidated(addr))
    return true;

  if (bootVerifySlot(addr) != OK)
    return true;

  flashRead(addr, (uint8_t *)&tag, sizeof(tag));
  p_info->valid   = true;
  p_info->fw_size = tag.fw_size;
  p_info->fw_crc  = tag.fw_crc;
  p_info->seq     = bootReadSlotMeta(addr, &slot) ? slot.seq : 0;

  return true;
}

bool bootGetFirmInfo(boot_slot_info_t *p_info)
{
  firm_tag_t tag;

  if (p_info == NULL)
    return false;

  memset(p_info, 0, sizeof(boot_slot_info_t));
  p_info->index = 0xFF;
  p_info->addr  = FLASH_ADDR_FIRM;

  if (bootVerifyFirm() != OK)
    return true;

  flashRead(FLASH_ADDR_FIRM, (uint8_t *)&tag, sizeof(tag));
  p_info->valid   = true;
  p_info->fw_size = tag.fw_size;
  p_info->fw_crc  = tag.fw_crc;

  // bootApplySlot() 이 boot_slot_t 까지 복사하므로 FIRM 도 seq 를 갖는다.
  // seq 가 없으면(SWD 로 직접 구운 경우) 0 이 되고, 그때는 롤백하지 않는다.
  {
    boot_slot_t slot;

    p_info->seq = bootReadSlotMeta(FLASH_ADDR_FIRM, &slot) ? slot.seq : 0;
  }

  return true;
}

uint32_t bootGetNextSeq(void)
{
  uint32_t seq = 0;

  for (uint8_t i = 0; i < FLASH_SLOT_MAX; i++)
  {
    boot_slot_info_t info;

    if (bootGetSlotInfo(i, &info) && info.valid && info.seq >= seq)
      seq = info.seq + 1;
  }
  if (seq == 0)
    seq = 1;

  return seq;
}

//-- 새 이미지를 받을 슬롯.
//
//   1순위 : 무효 슬롯
//   2순위 : FIRM 과 (fw_size, fw_crc) 가 불일치하는 슬롯  = 낡은 이미지
//   3순위 : seq 가 작은 슬롯
//
//   FIRM 과 일치하는 슬롯은 "지금 실행 중인 이미지의 백업본" 이므로 보존한다.
//
int8_t bootGetWriteSlot(void)
{
  boot_slot_info_t info[FLASH_SLOT_MAX];
  boot_slot_info_t firm;

  for (uint8_t i = 0; i < FLASH_SLOT_MAX; i++)
    bootGetSlotInfo(i, &info[i]);
  bootGetFirmInfo(&firm);

  for (uint8_t i = 0; i < FLASH_SLOT_MAX; i++)
  {
    if (info[i].valid == false)
      return (int8_t)i;
  }

  if (firm.valid)
  {
    for (uint8_t i = 0; i < FLASH_SLOT_MAX; i++)
    {
      if (info[i].fw_size != firm.fw_size || info[i].fw_crc != firm.fw_crc)
        return (int8_t)i;
    }
  }

  {
    uint8_t sel = 0;

    for (uint8_t i = 1; i < FLASH_SLOT_MAX; i++)
    {
      if (info[i].seq < info[sel].seq)
        sel = i;
    }
    return (int8_t)sel;
  }
}

//-- FIRM 에 아직 반영되지 않은 유효 슬롯 = 적용 대기
//
int8_t bootGetPendingSlot(void)
{
  boot_slot_info_t firm;
  int8_t   sel = -1;
  uint32_t sel_seq = 0;

  bootGetFirmInfo(&firm);

  for (uint8_t i = 0; i < FLASH_SLOT_MAX; i++)
  {
    boot_slot_info_t info;

    if (!bootGetSlotInfo(i, &info) || !info.valid)
      continue;

    if (firm.valid && info.fw_size == firm.fw_size && info.fw_crc == firm.fw_crc)
      continue;                       // 이미 FIRM 에 올라간 이미지

    // FIRM 보다 오래된(seq 가 작은) 슬롯은 '적용 대기' 가 아니라 롤백 대상이다.
    // 이 검사를 빠뜨리면 MODE_BIT_UPDATE 로 리셋했을 때 옛 버전으로 다운그레이드된다.
    if (firm.valid && info.seq <= firm.seq)
      continue;

    if (sel < 0 || info.seq >= sel_seq)
    {
      sel = (int8_t)i;
      sel_seq = info.seq;
    }
  }
  return sel;
}

//-- 롤백 대상.
//
//   FIRM 과 일치하는 슬롯이 하나도 없으면 -1 을 돌려 롤백하지 않는다.
//   SWD 로 FIRM 만 직접 구운 상태에서 엉뚱한 버전으로 되돌리는 것을 막는다.
//
int8_t bootGetRollbackSlot(void)
{
  boot_slot_info_t firm;
  int8_t   sel = -1;
  uint32_t sel_seq = 0;

  bootGetFirmInfo(&firm);

  // FIRM 이 무효하거나 seq 가 없으면(SWD 로 직접 구운 상태) 롤백하지 않는다.
  // 엉뚱한 버전으로 되돌리는 것이 아무것도 안 하는 것보다 나쁘다.
  if (!firm.valid || firm.seq == 0)
    return -1;

  for (uint8_t i = 0; i < FLASH_SLOT_MAX; i++)
  {
    boot_slot_info_t info;

    if (!bootGetSlotInfo(i, &info) || !info.valid)
      continue;

    // 현재 FIRM 과 같은 이미지는 롤백 대상이 아니다.
    if (info.fw_size == firm.fw_size && info.fw_crc == firm.fw_crc)
      continue;

    // FIRM 보다 오래된 것만 롤백 대상이다.
    if (info.seq >= firm.seq)
      continue;

    if (sel < 0 || info.seq >= sel_seq)
    {
      sel = (int8_t)i;
      sel_seq = info.seq;
    }
  }
  return sel;
}

//-- 슬롯 -> FIRM 복사.
//
//   전원 손실 대비로 기록 순서가 중요하다.
//     (1) 본문(0x400~)  (2) tag_crc(0x010)  (3) firm_tag_t 첫 쿼드워드(0x000) = 커밋
//   오프셋 0 부터 순차 복사하면 TAG 가 가장 먼저 기록되어
//   "TAG 유효 + 본문 미완성" 상태가 생긴다.
//
uint16_t bootApplySlot(uint8_t index)
{
  uint32_t   src = bootGetSlotAddr(index);
  firm_tag_t tag;
  uint16_t   err_code;
  uint8_t    buf[512] __attribute__((aligned(16)));

  if (index >= FLASH_SLOT_MAX)
    return ERR_BOOT_WRONG_RANGE;

  err_code = bootVerifySlot(src);
  if (err_code != OK)
    return err_code;

  flashRead(src, (uint8_t *)&tag, sizeof(tag));

  if ((FLASH_SIZE_TAG + tag.fw_size) > FLASH_SIZE_FIRM)
    return ERR_BOOT_TAG_SIZE;

  logPrintf("[  ] bootApplySlot(%d) %d KB\n", index, (int)(tag.fw_size/1024));

  if (flashErase(FLASH_ADDR_FIRM, FLASH_SIZE_TAG + tag.fw_size) != true)
    return ERR_BOOT_FLASH_ERASE;

  // (1) 본문
  for (uint32_t i = 0; i < tag.fw_size; i += sizeof(buf))
  {
    uint32_t n = tag.fw_size - i;

    if (n > sizeof(buf))
      n = sizeof(buf);

    if (flashRead(src + FLASH_SIZE_TAG + i, buf, n) != true)
      return ERR_BOOT_FLASH_READ;
    if (flashWrite(FLASH_ADDR_FIRM + FLASH_SIZE_TAG + i, buf, n) != true)
      return ERR_BOOT_FLASH_WRITE;

    ledToggle(_DEF_LED1);
  }

  // (2) boot_slot_t : FIRM 도 seq 를 가져야 적용 대기/롤백을 구분할 수 있다
  if (flashRead(src + BOOT_SLOT_TAG_OFFSET, buf, 16) != true)
    return ERR_BOOT_FLASH_READ;
  if (flashWrite(FLASH_ADDR_FIRM + BOOT_SLOT_TAG_OFFSET, buf, 16) != true)
    return ERR_BOOT_FLASH_WRITE;

  // (3) tag_crc 가 든 두 번째 쿼드워드
  if (flashRead(src + 0x10, buf, 16) != true)
    return ERR_BOOT_FLASH_READ;
  if (flashWrite(FLASH_ADDR_FIRM + 0x10, buf, 16) != true)
    return ERR_BOOT_FLASH_WRITE;

  // (4) 커밋 : magic/fw_addr/fw_size/fw_crc 가 한 쿼드워드에 들어간다
  if (flashRead(src + 0x00, buf, 16) != true)
    return ERR_BOOT_FLASH_READ;
  if (flashWrite(FLASH_ADDR_FIRM + 0x00, buf, 16) != true)
    return ERR_BOOT_FLASH_WRITE;

  return bootVerifyFirm();
}

//-- 슬롯 무효화.
//
//   (1) 무효 마커 쿼드워드 기록(약 40us)  (2) 첫 섹터 소거(약 2ms)
//   (1) 만 성공해도 무효로 판정되므로 소거 중 전원이 끊겨도 안전하다.
//
uint16_t bootInvalidateSlot(uint8_t index)
{
  uint32_t addr = bootGetSlotAddr(index);
  uint8_t  buf[16] __attribute__((aligned(16)));
  uint32_t magic = BOOT_SLOT_INV_MAGIC;

  if (index >= FLASH_SLOT_MAX)
    return ERR_BOOT_WRONG_RANGE;

  memset(buf, 0xFF, sizeof(buf));
  memcpy(buf, &magic, sizeof(magic));
  flashWrite(addr + BOOT_SLOT_INV_OFFSET, buf, sizeof(buf));   // 실패해도 계속 진행

  if (flashErase(addr, FLASH_SECTOR_SIZE) != true)
    return ERR_BOOT_FLASH_ERASE;

  return OK;
}

uint16_t bootJumpFirm(void)
{
  uint16_t err_code;

  err_code = bootVerifyFirm();
  if (err_code != OK)
    return err_code;

  {
    // 포인터 폭이 32비트가 아닌 호스트 유닛 테스트에서도 경고 없이 좁혀지도록
    // uintptr_t 를 거친다. 타깃에서는 둘 다 32비트라 결과가 같다.
    void (**jump_func)(void) = (void (**)(void))(FLASH_ADDR_FIRM + FLASH_SIZE_TAG + 4);
    uint32_t reset_handler = (uint32_t)(uintptr_t)(*jump_func);

    if (reset_handler < FLASH_ADDR_FIRM ||
        reset_handler >= (FLASH_ADDR_FIRM + FLASH_SIZE_FIRM))
    {
      logPrintf("[E_] ERR_BOOT_INVALID_FW 0x%X\n", reset_handler);
      return ERR_BOOT_INVALID_FW;
    }

    logPrintf("[  ] bootJumpFirm()\n");
    logPrintf("     addr : 0x%X\n", reset_handler);

    resetSetBootMode(0);
    bspDeInit();

    // VTOR/MSP 는 건드리지 않는다.
    //  - MSP  : 앱 Reset_Handler 의 ldr sp, =_estack
    //  - VTOR : 앱 SystemInit() 의 SCB->VTOR = &_fw_flash_begin
    (*jump_func)();
  }

  return OK;   // 도달하지 않는다
}


#if CLI_USE(HW_BOOT)
static void bootPrintVer(const char *p_name, uint32_t ver_addr)
{
  firm_ver_t ver;

  flashRead(ver_addr, (uint8_t *)&ver, sizeof(ver));
  if (ver.magic_number == VERSION_MAGIC_NUMBER)
  {
    cliPrintf("  %-6s %.*s  %.*s\n", p_name,
              (int)sizeof(ver.name_str), ver.name_str,
              (int)sizeof(ver.version_str), ver.version_str);
  }
  else
  {
    cliPrintf("  %-6s -\n", p_name);
  }
}

void cliBoot(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    boot_slot_info_t firm;

    cliPrintf("BOOT\n");
    bootPrintVer("BOOT", FLASH_ADDR_BOOT + FLASH_SIZE_VEC);

    bootGetFirmInfo(&firm);
    cliPrintf("FIRM   0x%08X valid=%d seq=%d size=%d crc=0x%04X (err 0x%X)\n",
              firm.addr, firm.valid, (int)firm.seq, (int)firm.fw_size, firm.fw_crc, bootVerifyFirm());
    if (firm.valid)
      bootPrintVer("", FLASH_ADDR_FIRM + FLASH_SIZE_TAG + FLASH_SIZE_VEC);

    for (uint8_t i = 0; i < FLASH_SLOT_MAX; i++)
    {
      boot_slot_info_t info;

      bootGetSlotInfo(i, &info);
      cliPrintf("SLOT%d  0x%08X valid=%d seq=%d size=%d crc=0x%04X\n",
                i, info.addr, info.valid, (int)info.seq,
                (int)info.fw_size, info.fw_crc);
      if (info.valid)
        bootPrintVer("", info.addr + FLASH_SIZE_TAG + FLASH_SIZE_VEC);
    }

    cliPrintf("\n");
    cliPrintf("write slot    : %d\n", bootGetWriteSlot());
    cliPrintf("pending slot  : %d\n", bootGetPendingSlot());
    cliPrintf("rollback slot : %d\n", bootGetRollbackSlot());
    cliPrintf("next seq      : %d\n", (int)bootGetNextSeq());
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "apply"))
  {
    uint8_t  n = (uint8_t)args->getData(1);
    uint32_t pre_time = millis();
    uint16_t err_code = bootApplySlot(n);

    cliPrintf("bootApplySlot(%d) : err 0x%X  %d ms\n", n, err_code, millis()-pre_time);
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "invalid"))
  {
    uint8_t n = (uint8_t)args->getData(1);

    cliPrintf("bootInvalidateSlot(%d) : err 0x%X\n", n, bootInvalidateSlot(n));
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "verify"))
  {
    cliPrintf("FIRM  : err 0x%X\n", bootVerifyFirm());
    for (uint8_t i = 0; i < FLASH_SLOT_MAX; i++)
      cliPrintf("SLOT%d : err 0x%X\n", i, bootVerifySlot(bootGetSlotAddr(i)));
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "log"))
  {
    static const char *evt_str[] =
      {"?", "UPDATE", "ROLLBACK", "FAULT_RECOVER", "VERIFY_FAIL", "ECC_CLEAN"};
    uint16_t n = bootLogGetCount();

    cliPrintf("boot log : %d records (max %d)\n", n, BOOT_LOG_REC_MAX * BOOT_LOG_SECTOR_MAX);
    cliPrintf("IDX  SEQ  TIME                 EVENT          SLOT  FROM    TO      DETAIL\n");
    for (uint16_t i = 0; i < n; i++)
    {
      boot_log_t log;

      if (bootLogRead(i, &log) != true)
        continue;

      cliPrintf("%3d %4d  ", i, (int)log.seq);

      // RTC 를 모르는 구간이 있다(코인셀이 없어 전원을 뽑으면 초기화된다).
      // 그때는 0 이 들어 있으므로 '-' 로 보여준다.
      {
        rtc_info_t t;

        if (rtcEpochToInfo(log.timestamp, &t) != true)
        {
          cliPrintf("%-19s ", "-");
        }
        else
        {
          cliPrintf("%04d-%02d-%02d %02d:%02d:%02d ",
                    2000 + t.date.year, t.date.month, t.date.day,
                    t.time.hours, t.time.minutes, t.time.seconds);
        }
      }

      cliPrintf("%-13s ", (log.event < 6) ? evt_str[log.event] : "?");
      if (log.slot == 0xFF)
        cliPrintf(" -   ");
      else
        cliPrintf(" %d   ", log.slot);
      cliPrintf("0x%04X  0x%04X  ", (unsigned int)log.from_crc, (unsigned int)log.to_crc);
      if (log.fault_pc)
        cliPrintf("PC=0x%08X ", (unsigned int)log.fault_pc);
      cliPrintf("rst=0x%X\n", (unsigned int)log.reset_bits);
    }
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "log") && args->isStr(1, "clear"))
  {
    cliPrintf("bootLogClear() : %d\n", bootLogClear());
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "jump"))
  {
    uint16_t err_code = bootJumpFirm();

    cliPrintf("bootJumpFirm() : err 0x%X\n", err_code);
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("boot info\n");
    cliPrintf("boot log [clear]\n");
    cliPrintf("boot verify\n");
    cliPrintf("boot apply   slot\n");
    cliPrintf("boot invalid slot\n");
    cliPrintf("boot jump\n");
  }
}
#endif
