#include "uf2.h"
#include "cli.h"


#if CLI_USE(HW_BOOT)
static void cliUf2(cli_args_t *args);
#endif

static bool     is_init      = false;
static bool     is_done_req  = false;
static bool     is_jump_req  = false;
static bool     is_tr_active = false;

static int8_t   tr_slot      = -1;        // 이번 전송이 쓰는 슬롯
static uint32_t tr_family    = 0;
static uint32_t flash_len    = 0;         // 기록된 최대 끝 오프셋
static uint8_t  percent      = 0;

static uint8_t  erase_map[UF2_ERASE_SECTOR_MAX / 8];


static void uf2TransferReset(WriteState *state);
static bool uf2FlashEraseOnce(uint32_t flash_addr, uint32_t len);
static bool uf2FlashWrite(uint32_t addr, void const *data, uint32_t len);
static bool uf2FlashFlush(void);
static bool uf2CalcCrc(uint32_t addr, uint32_t length, uint16_t *p_crc);



bool uf2Init(void)
{
  is_init      = true;
  is_done_req  = false;
  is_jump_req  = false;
  is_tr_active = false;
  tr_slot      = -1;
  flash_len    = 0;
  percent      = 0;
  memset(erase_map, 0, sizeof(erase_map));

  logPrintf("[OK] uf2Init()\n");
  logPrintf("     familyID  : 0x%08X\n", (unsigned int)BOARD_UF2_FAMILY_ID);
  logPrintf("     max fw    : %d KB\n", (int)(UF2_MAX_FW_SIZE/1024));

#if CLI_USE(HW_BOOT)
  cliAdd("uf2", cliUf2);
#endif
  return true;
}

bool uf2IsBusy(void)
{
  return is_tr_active || is_done_req;
}

uint8_t uf2GetPercent(void)
{
  return percent;
}

void uf2RequestJump(void)
{
  is_jump_req = true;
}

static inline bool uf2IsBlock(UF2_Block const *bl)
{
  return (bl->magicStart0 == UF2_MAGIC_START0) &&
         (bl->magicStart1 == UF2_MAGIC_START1) &&
         (bl->magicEnd    == UF2_MAGIC_END) &&
         (bl->flags & UF2_FLAG_FAMILYID) &&
         !(bl->flags & UF2_FLAG_NOFLASH);
}

void uf2TransferReset(WriteState *state)
{
  memset(state, 0, sizeof(WriteState));
  memset(erase_map, 0, sizeof(erase_map));
  flash_len = 0;
  percent   = 0;
}

//-- 슬롯 안의 섹터를 전송당 한 번만 지운다.
//
bool uf2FlashEraseOnce(uint32_t flash_addr, uint32_t len)
{
  uint32_t base = bootGetSlotAddr((uint8_t)tr_slot);
  uint32_t offset;
  uint32_t sector_s;
  uint32_t sector_e;

  if (tr_slot < 0)
    return false;

  if (flash_addr < base || (flash_addr + len) > (base + FLASH_SIZE_SLOT))
    return false;

  offset   = flash_addr - base;
  sector_s = offset / UF2_ERASE_SECTOR_SIZE;
  sector_e = (offset + len - 1) / UF2_ERASE_SECTOR_SIZE;

  for (uint32_t i = sector_s; i <= sector_e; i++)
  {
    uint8_t  mask = 1 << (i % 8);
    uint32_t pos  = i / 8;

    if (erase_map[pos] & mask)
      continue;

    erase_map[pos] |= mask;

    if (flashErase(base + i * UF2_ERASE_SECTOR_SIZE, UF2_ERASE_SECTOR_SIZE) != true)
    {
      logPrintf("[E_] uf2 erase 0x%X\n", (unsigned int)(base + i * UF2_ERASE_SECTOR_SIZE));
      return false;
    }
  }
  return true;
}

//-- targetAddr 는 0-베이스 오프셋이다 (uf2conv.py --convert / --base 0x0).
//   실제 기록 위치는 슬롯 베이스 + TAG(1KB) + targetAddr 이다.
//
bool uf2FlashWrite(uint32_t addr, void const *data, uint32_t len)
{
  uint32_t base = bootGetSlotAddr((uint8_t)tr_slot);
  uint32_t flash_addr;

  if ((addr + len) > UF2_MAX_FW_SIZE)
  {
    logPrintf("[E_] uf2 size over 0x%X\n", (unsigned int)(addr + len));
    return false;
  }
  if ((addr % 16) != 0)
    return false;

  flash_addr = base + FLASH_SIZE_TAG + addr;

  if (uf2FlashEraseOnce(flash_addr, len) != true)
    return false;
  if (flashWrite(flash_addr, (uint8_t *)data, len) != true)
    return false;

  // 블록이 순서대로 오지 않을 수 있으므로 누적이 아니라 최대 끝 오프셋으로 관리한다.
  if ((addr + len) > flash_len)
    flash_len = addr + len;

  return true;
}

bool uf2CalcCrc(uint32_t addr, uint32_t length, uint16_t *p_crc)
{
  uint8_t  buf[256];
  uint16_t crc = 0;
  uint32_t index = 0;

  while (index < length)
  {
    uint32_t rd_len = length - index;

    if (rd_len > sizeof(buf))
      rd_len = sizeof(buf);

    if (flashRead(addr + index, buf, rd_len) != true)
      return false;

    crc = utilCalcCRC(crc, buf, rd_len);
    index += rd_len;
  }
  *p_crc = crc;
  return true;
}

//-- 전송 완료. 태그를 기록한다.
//
//   기록 순서가 전원 손실 대응의 핵심이다.
//     (1) boot_slot_t (0x020)
//     (2) firm_tag_t 뒷 쿼드워드 tag_crc (0x010)
//     (3) firm_tag_t 첫 쿼드워드 (0x000)  <- 커밋
//
//   (3) 하나가 원자적으로 magic/fw_addr/fw_size/fw_crc 를 모두 확정시킨다.
//   중간에 끊기면 TAG 매직이 없어 슬롯이 무효로 판정되고, 반대편 슬롯(현재 FIRM 의
//   백업본)은 온전하다.
//
bool uf2FlashFlush(void)
{
  uint32_t    base = bootGetSlotAddr((uint8_t)tr_slot);
  uint16_t    crc = 0;
  uint8_t     buf[16] __attribute__((aligned(16)));
  firm_tag_t  tag;
  boot_slot_t slot;

  if (tr_slot < 0 || flash_len == 0)
    return false;

  if (uf2CalcCrc(base + FLASH_SIZE_TAG, flash_len, &crc) != true)
    return false;

  tag.magic_number = TAG_MAGIC_NUMBER;
  tag.fw_addr      = FLASH_SIZE_TAG;
  tag.fw_size      = flash_len;
  tag.fw_crc       = crc;
  tag.tag_crc      = 0;

  // (1) boot_slot_t
  slot.magic = BOOT_SLOT_MAGIC;
  slot.seq   = bootGetNextSeq();
  slot.flags = BOOT_SLOT_FLAG_NONE;
  slot.crc   = utilCalcCRC(0, (uint8_t *)&slot, 12);
  memset(buf, 0xFF, sizeof(buf));
  memcpy(buf, &slot, sizeof(slot));
  if (flashWrite(base + BOOT_SLOT_TAG_OFFSET, buf, sizeof(buf)) != true)
    return false;

  // (2) tag_crc 가 든 두 번째 쿼드워드
  tag.tag_crc = utilCalcCRC(0, (uint8_t *)&tag, 16);
  memset(buf, 0xFF, sizeof(buf));
  memcpy(buf, &tag.tag_crc, sizeof(tag.tag_crc));
  if (flashWrite(base + 0x10, buf, sizeof(buf)) != true)
    return false;

  // (3) 커밋
  memset(buf, 0xFF, sizeof(buf));
  memcpy(buf, &tag, 16);
  if (flashWrite(base + 0x00, buf, sizeof(buf)) != true)
    return false;

  logPrintf("[  ] uf2 flush slot%d size=%d crc=0x%04X seq=%d\n",
            tr_slot, (int)flash_len, crc, (int)slot.seq);

  return (bootVerifySlot(base) == OK);
}

//-- 512B 섹터 하나를 UF2 블록으로 해석한다.
//
int uf2WriteBlock(uint32_t block_no, uint8_t *data, WriteState *state)
{
  UF2_Block *bl = (void *)data;
  bool is_new_block = true;

  (void)block_no;

  if (!uf2IsBlock(bl))
    return UF2_RET_NOT_UF2;

  if (bl->familyID != BOARD_UF2_FAMILY_ID)
  {
    logPrintf("[E_] familyID 0x%X != 0x%X\n",
              (unsigned int)bl->familyID, (unsigned int)BOARD_UF2_FAMILY_ID);
    return UF2_RET_NOT_UF2;
  }

  // 새 전송 판정. familyID 가 바뀌었거나 총 블록 수가 다르면 다른 파일이다.
  //
  if (is_tr_active == false || bl->familyID != tr_family ||
      (bl->numBlocks > 0 && state->numBlocks > 0 && bl->numBlocks != state->numBlocks))
  {
    uf2TransferReset(state);

    tr_slot = bootGetWriteSlot();
    if (tr_slot < 0)
    {
      logPrintf("[E_] uf2 no write slot\n");
      return UF2_RET_ERR;
    }

    is_tr_active = true;
    tr_family    = bl->familyID;
    logPrintf("[  ] uf2 begin -> slot%d (%d blocks)\n", tr_slot, (int)bl->numBlocks);
  }

  if (bl->numBlocks > 0 && bl->numBlocks < MAX_BLOCKS)
    state->numBlocks = bl->numBlocks;

  // 호스트가 같은 블록을 다시 보내는 경우가 있다. 다시 기록하면 길이/CRC 가
  // 두 번 반영되고 erase 까지 다시 걸릴 수 있으므로 이미 받은 블록은 건너뛴다.
  //
  if (state->numBlocks > 0 && bl->blockNo < MAX_BLOCKS)
  {
    uint8_t const mask = 1 << (bl->blockNo % 8);
    uint32_t const pos = bl->blockNo / 8;

    if (state->writtenMask[pos] & mask)
    {
      is_new_block = false;
    }
    else
    {
      state->writtenMask[pos] |= mask;
      state->numWritten++;
    }
  }

  if (is_new_block)
  {
    if (uf2FlashWrite(bl->targetAddr, bl->data, bl->payloadSize) != true)
    {
      state->aborted = true;
      is_tr_active = false;
      return UF2_RET_ERR;
    }
    ledToggle(_DEF_LED1);
  }

  if (state->numBlocks > 0)
    percent = (uint8_t)((state->numWritten * 100) / state->numBlocks);

  return UF2_DISK_BLOCK_SIZE;
}

bool uf2FlashComplete(WriteState *state)
{
  if (state->aborted)
    return false;

  // 호스트는 전송이 끝난 뒤에도 FAT/디렉터리 갱신으로 write10 을 더 보낸다.
  // 그때마다 complete_cb 가 다시 불리므로, 한 번 마무리한 전송을 재실행하지
  // 않도록 여기서 막는다. 막지 않으면 이미 기록한 태그에 재기록을 시도해
  // (STM32H5 는 쿼드워드 재기록 불가) 실패 로그가 쏟아진다.
  //
  if (is_done_req)
    return true;

  if (uf2FlashFlush() != true)
  {
    logPrintf("[E_] uf2FlashFlush()\n");
    is_tr_active = false;
    state->numBlocks = 0;
    return false;
  }

  is_tr_active = false;
  state->numBlocks = 0;      // 같은 조건으로 재진입하지 않도록 초기화
  is_done_req  = true;
  return true;
}

//-- 완료 후 상태머신. USB 콜백 밖(메인 루프)에서 실행된다.
//
void uf2Update(void)
{
  static uint8_t  state = 0;
  static uint32_t pre_time = 0;

  if (is_init != true)
    return;

  switch (state)
  {
    case 0:
      if (is_done_req)
      {
        pre_time = millis();
        state = 1;
      }
      else if (is_jump_req)
      {
        pre_time = millis();
        state = 3;
      }
      break;

    case 1:
      // 호스트가 FAT/디렉터리 기록과 SYNCHRONIZE_CACHE 를 마칠 시간을 준다.
      if (millis() - pre_time >= UF2_COMPLETE_WAIT_MS)
      {
        // bootApplySlot() 은 뱅크1 소거/기록으로 수 초가 걸리고 그동안 USB 가
        // 응답하지 못한다. 호스트가 장치 분리를 정상 인식하도록 먼저 끊는다.
        usbDisconnect();
        state = 2;
      }
      break;

    case 2:
      {
        int8_t   slot = bootGetPendingSlot();
        uint16_t err_code = ERR_BOOT_INVALID_FW;

        if (slot >= 0)
          err_code = bootApplySlot((uint8_t)slot);

        if (err_code == OK)
        {
          pre_time = millis();
          state = 3;
        }
        else
        {
          // 실패해도 벽돌이 되지 않는다. USB 를 다시 붙여 재복사할 수 있게 한다.
          logPrintf("[E_] bootApplySlot() err 0x%04X\n", err_code);
          usbConnect();
          is_done_req = false;
          state = 0;
        }
      }
      break;

    case 3:
      if (millis() - pre_time >= UF2_JUMP_WAIT_MS)
      {
        is_done_req = false;
        is_jump_req = false;
        bootJumpFirm();
        state = 0;
      }
      break;
  }
}


#if CLI_USE(HW_BOOT)
//-- UF2 / MSC 시험 명령.
//
//   `uf2 eject` 는 "미디어가 빠졌다" 고 보고한다. macOS 가 UF2 복사 후에
//   "디스크를 제대로 꺼내지 않았습니다" 를 띄우는데, USB 를 끊기 전에 이걸 먼저
//   보고하면 조용히 언마운트될 수 있다. 효과가 OS 마다 달라서 자동 흐름에 넣기
//   전에 사람이 직접 확인할 수 있게 분리해뒀다.
//
//   시험 방법
//     1) 리셋 더블클릭으로 부트 모드 진입 -> H5BOOT 볼륨이 마운트된다
//     2) SWD CLI 에서 `uf2 eject`
//     3) 경고 없이 볼륨이 사라지면 성공. 뜨면 이 방법으로는 안 되는 것이다
//     4) `uf2 insert` 로 되돌린다 (호스트가 다시 붙인다)
//
void cliUf2(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("familyID  : 0x%08X\n", (unsigned int)BOARD_UF2_FAMILY_ID);
    cliPrintf("max fw    : %d KB\n", (int)(UF2_MAX_FW_SIZE/1024));
    cliPrintf("busy      : %d\n", uf2IsBusy());
    cliPrintf("percent   : %d %%\n", uf2GetPercent());
    cliPrintf("medium    : %s\n", uf2DiskGetMedium() ? "present" : "not present");
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "eject"))
  {
    uf2DiskSetMedium(false);
    cliPrintf("medium not present 로 보고한다.\n");
    cliPrintf("호스트가 볼륨을 내리는지 본다. 경고가 뜨면 이 방법은 소용이 없다.\n");
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "insert"))
  {
    uf2DiskSetMedium(true);
    cliPrintf("medium present 로 되돌렸다.\n");
    ret = true;
  }

  //   실제 UF2 완료 흐름과 같은 순서로 시험한다.
  //   eject 보고 -> 잠깐 대기 -> USB 분리. 이 조합에서 경고가 뜨는지가 관건이다.
  if (args->argc == 1 && args->isStr(0, "unplug"))
  {
    uf2DiskSetMedium(false);
    cliPrintf("eject 보고 후 500ms 대기...\n");
    delay(500);
    usbDisconnect();
    cliPrintf("usbDisconnect(). `uf2 plug` 로 다시 붙인다.\n");
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "plug"))
  {
    uf2DiskSetMedium(true);
    usbConnect();
    cliPrintf("usbConnect()\n");
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("uf2 info\n");
    cliPrintf("uf2 eject     - medium not present 로 보고\n");
    cliPrintf("uf2 insert    - 되돌리기\n");
    cliPrintf("uf2 unplug    - eject 보고 + 500ms + USB 분리 (실제 흐름과 동일)\n");
    cliPrintf("uf2 plug      - USB 재연결\n");
  }
}
#endif
