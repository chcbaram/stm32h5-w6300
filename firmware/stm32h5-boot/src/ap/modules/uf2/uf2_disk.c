#include "uf2.h"

#ifdef _USE_HW_USB
#if CFG_TUD_MSC

#include "usb.h"


//-- 손으로 만든 FAT16 정적 디스크.
//
//   실제 RAM 은 부트섹터/FAT/루트디렉터리 3섹터만 잡고, 나머지는 read10 에서
//   0 으로 채워 응답한다.
//
//   총 32768 섹터 x 512B = 16MB, 클러스터당 1섹터.
//   FAT 스펙상 총 섹터가 0x10000 미만이면 16bit 필드를 쓰고 32bit 필드는 0 이어야 한다.
//
#define DISK_BLOCK_NUM      UF2_DISK_BLOCK_NUM
#define DISK_BLOCK_SIZE     UF2_DISK_BLOCK_SIZE

#define LBA_BOOT            0
#define LBA_FAT1            1           // 1 ~ 128
#define LBA_FAT2            129         // 129 ~ 256
#define LBA_ROOT            257         // 257 ~ 288
#define LBA_README          289         // 클러스터 2

#define SCSI_CMD_SYNC_CACHE_10  0x35
#define SCSI_CMD_SYNC_CACHE_16  0x91


static uint8_t  boot_sector[DISK_BLOCK_SIZE] =
{
  0xEB, 0x3C, 0x90,
  'M','S','D','O','S','5','.','0',
  0x00, 0x02,             // Bytes per sector (512)
  0x01,                   // Sectors per cluster (1)
  0x01, 0x00,             // Reserved sectors (1)
  0x02,                   // FAT count (2)
  0x00, 0x02,             // Root entries (512 -> 32 섹터)
  0x00, 0x80,             // [19-20] Total sectors 16 (32768 = 0x8000)
  0xF8,                   // Media descriptor
  0x80, 0x00,             // [22-23] Sectors per FAT (128)
  0x01, 0x00,             // Sectors per track
  0x01, 0x00,             // Heads
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, // [32-35] Total sectors 32 (16bit 필드를 썼으므로 0)
  0x80, 0x00, 0x29,
  0x34, 0x12, 0xCD, 0xAB, // Volume serial
  'H','5','B','O','O','T',' ',' ',' ',' ',' ',
  'F','A','T','1','6',' ',' ',' ',
  [510] = 0x55, [511] = 0xAA
};

static uint8_t fat_table[DISK_BLOCK_SIZE] =
{
  0xF8, 0xFF,             // Media descriptor
  0xFF, 0xFF,             // EOC
  0xFF, 0xFF,             // Cluster 2 (README.TXT) - EOC
};

static uint8_t root_dir[DISK_BLOCK_SIZE] =
{
  // 볼륨 라벨
  'H','5','B','O','O','T',' ',' ',' ',' ',' ', 0x08, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4F, 0x6D, 0x65, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  // README.TXT
  'R','E','A','D','M','E',' ',' ','T','X','T', 0x20, 0x00, 0xC6, 0x52, 0x6D,
  0x65, 0x43, 0x65, 0x43, 0x00, 0x00, 0x88, 0x6D, 0x65, 0x43, 0x02, 0x00,
  0x00, 0x00, 0x00, 0x00,   // 파일 크기 : read10 에서 실제 길이로 갱신
};

static char       readme_txt[512];
static uint32_t   readme_len = 0;
static WriteState wr_state = {0};


//-- README.TXT 를 현재 상태로 만들어 준다.
//
static void uf2MakeReadme(void)
{
  uint32_t index = 0;
  int      len;

  #define README_ADD(...)                                                     \
    do {                                                                      \
      len = snprintf(&readme_txt[index], sizeof(readme_txt)-index, __VA_ARGS__); \
      if (len > 0) index += (uint32_t)len;                                    \
      if (index > sizeof(readme_txt)-1) index = sizeof(readme_txt)-1;         \
    } while (0)

  README_ADD("%s\r\n\r\n", UF2_PRODUCT_NAME);
  README_ADD("Board   : %s\r\n", UF2_BOARD_ID);
  README_ADD("Boot Ver: %s\r\n", _DEF_FIRMWATRE_VERSION);
  README_ADD("FamilyID: 0x%08X\r\n", (unsigned int)BOARD_UF2_FAMILY_ID);
  README_ADD("Max FW  : %d KB\r\n\r\n", (int)(UF2_MAX_FW_SIZE/1024));

  {
    firm_ver_t ver;
    boot_slot_info_t firm;

    bootGetFirmInfo(&firm);
    flashRead(FLASH_ADDR_FIRM + FLASH_SIZE_TAG + FLASH_SIZE_VEC,
              (uint8_t *)&ver, sizeof(ver));

    if (firm.valid && ver.magic_number == VERSION_MAGIC_NUMBER)
    {
      // 플래시에서 읽은 문자열은 NUL 종료가 보장되지 않는다.
      README_ADD("FIRM    : %.*s\r\n", (int)sizeof(ver.name_str), ver.name_str);
      README_ADD("          %.*s\r\n", (int)sizeof(ver.version_str), ver.version_str);
      README_ADD("          %d KB  crc 0x%04X\r\n", (int)(firm.fw_size/1024), (unsigned int)firm.fw_crc);
    }
    else
    {
      README_ADD("FIRM    : (none)\r\n");
    }
  }

  for (uint8_t i = 0; i < FLASH_SLOT_MAX; i++)
  {
    boot_slot_info_t info;

    bootGetSlotInfo(i, &info);
    if (info.valid)
      README_ADD("SLOT%d   : seq %d  %d KB  crc 0x%04X\r\n",
                 i, (int)info.seq, (int)(info.fw_size/1024), (unsigned int)info.fw_crc);
    else
      README_ADD("SLOT%d   : (empty)\r\n", i);
  }

  README_ADD("\r\nDrop a .uf2 file here to update.\r\n");

  readme_len = index;
}


//--------------------------------------------------------------------+
// MSC callbacks
//--------------------------------------------------------------------+
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4])
{
  (void)lun;
  const char vid[] = "BARAM";
  const char pid[] = "Boot Storage";
  const char rev[] = "1.0";

  memset(vendor_id,  ' ', 8);
  memset(product_id, ' ', 16);
  memset(product_rev,' ', 4);
  memcpy(vendor_id,  vid, strlen(vid));
  memcpy(product_id, pid, strlen(pid));
  memcpy(product_rev, rev, strlen(rev));
}

bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  (void)lun;
  return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
  (void)lun;
  *block_count = DISK_BLOCK_NUM;
  *block_size  = DISK_BLOCK_SIZE;
}

bool tud_msc_is_writable_cb(uint8_t lun)
{
  (void)lun;
  return true;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition,
                           bool start, bool load_eject)
{
  (void)lun; (void)power_condition;

  if (load_eject && !start)
  {
    // 여기서 바로 점프하면 이 명령의 CSW 를 호스트에 보내기 전에 USB 가 죽어
    // Windows 에서 "장치 제거 오류" 가 뜬다. 요청만 남기고 uf2Update() 에서 처리한다.
    uf2RequestJump();
  }
  return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize)
{
  (void)lun;

  memset(buffer, 0, bufsize);      // 데이터가 없는 영역 대응

  if (lba == LBA_BOOT)
  {
    if (offset < sizeof(boot_sector))
    {
      uint32_t n = sizeof(boot_sector) - offset;
      if (n > bufsize) n = bufsize;
      memcpy(buffer, &boot_sector[offset], n);
    }
  }
  else if (lba == LBA_FAT1 || lba == LBA_FAT2)
  {
    if (offset < sizeof(fat_table))
    {
      uint32_t n = sizeof(fat_table) - offset;
      if (n > bufsize) n = bufsize;
      memcpy(buffer, &fat_table[offset], n);
    }
  }
  else if (lba == LBA_ROOT)
  {
    // README.TXT 는 내용이 펌웨어 상태에 따라 달라진다. 디렉터리 엔트리의
    // 파일 크기를 실제 길이와 맞춰야 뒤에 NUL 이 붙거나 잘리지 않는다.
    // 두 번째 엔트리(오프셋 32)의 크기 필드는 +28 위치다.
    uf2MakeReadme();
    root_dir[32 + 28] = (readme_len >>  0) & 0xFF;
    root_dir[32 + 29] = (readme_len >>  8) & 0xFF;
    root_dir[32 + 30] = (readme_len >> 16) & 0xFF;
    root_dir[32 + 31] = (readme_len >> 24) & 0xFF;

    if (offset < sizeof(root_dir))
    {
      uint32_t n = sizeof(root_dir) - offset;
      if (n > bufsize) n = bufsize;
      memcpy(buffer, &root_dir[offset], n);
    }
  }
  else if (lba == LBA_README)
  {
    uf2MakeReadme();
    if (offset < readme_len)
    {
      uint32_t n = readme_len - offset;
      if (n > bufsize) n = bufsize;
      memcpy(buffer, &readme_txt[offset], n);
    }
  }

  return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize)
{
  (void)lun; (void)offset;
  uint32_t count = 0;

  while (count < bufsize)
  {
    int ret = uf2WriteBlock(lba, buffer, &wr_state);

    if (ret == 0)
      break;      // 플래시 기록 중

    // 기록 실패는 호스트에 알려야 한다. 성공으로 응답하면 복사가 끝난 것처럼
    // 보이지만 실제로는 플래시에 아무것도 남지 않는다.
    if (ret == UF2_RET_ERR)
      return -1;

    // UF2 가 아닌 블록(FAT/디렉터리 기록)은 성공으로 처리한다.
    lba++;
    buffer += DISK_BLOCK_SIZE;
    count  += DISK_BLOCK_SIZE;
  }
  return (int32_t)count;
}

void tud_msc_write10_complete_cb(uint8_t lun)
{
  (void)lun;

  if (wr_state.aborted)
  {
    logPrintf("[E_] uf2 aborted\n");
  }
  else if (wr_state.numBlocks)
  {
    if (wr_state.numWritten >= wr_state.numBlocks)
    {
      uf2FlashComplete(&wr_state);
    }
  }
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16],
                        void *buffer, uint16_t bufsize)
{
  (void)buffer; (void)bufsize;

  switch (scsi_cmd[0])
  {
    // Windows 는 쓰기 후/제거 시 이 명령을 보낸다. tinyusb 내장 처리 목록에 없어
    // 여기까지 내려오는데, 실패로 응답하면 "지연된 쓰기 실패" 류의 복사 오류가 난다.
    case SCSI_CMD_SYNC_CACHE_10:
    case SCSI_CMD_SYNC_CACHE_16:
      return 0;

    default:
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
      return -1;
  }
}

#endif
#endif
