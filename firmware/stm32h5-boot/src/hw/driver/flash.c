#include "flash.h"


#ifdef _USE_HW_FLASH
#include "cli.h"


//-- STM32H563 내부 플래시
//   뱅크당 1MB, 섹터 8KB × 128, 프로그램 단위 16바이트(쿼드워드)
//
//   주의: FLASH_BANK_SIZE 매크로는 FLASHSIZE_BASE 레지스터를 런타임에 읽는
//         식이라 상수식(배열 크기, #if)에 쓸 수 없다. 리터럴을 사용한다.
//
#define FLASH_BASE_ADDR           0x08000000UL
#define FLASH_BANK_BYTES          0x00100000UL          // 1MB
#define FLASH_TOTAL_BYTES         (FLASH_BANK_BYTES * 2)
#define FLASH_SECTOR_BYTES        FLASH_SECTOR_SIZE     // 0x2000 (8KB)
#define FLASH_SECTOR_PER_BANK     128
#define FLASH_WRITE_SIZE          16                    // QUADWORD


static bool is_init       = false;
static bool is_swap_bank  = false;

static bool flashGetBankSector(uint32_t addr, uint32_t *p_bank, uint32_t *p_sector);
static bool flashInRange(uint32_t addr, uint32_t length);
static bool flashIsProtected(uint32_t addr, uint32_t length);
static void flashCacheInvalidate(void);
static bool flashIsBlank(uint32_t addr, uint32_t length);

#if CLI_USE(HW_FLASH)
static void cliFlash(cli_args_t *args);
#endif




bool flashInit(void)
{
  is_swap_bank = (FLASH->OPTSR_CUR & FLASH_OPTSR_SWAP_BANK) ? true : false;

  is_init = true;

  logPrintf("[OK] flashInit()\n");
  logPrintf("     Sector : %d KB x %d x 2bank\n",
            (int)(FLASH_SECTOR_BYTES/1024), FLASH_SECTOR_PER_BANK);
  if (is_swap_bank)
  {
    logPrintf("     [!!] SWAP_BANK enabled\n");
  }

#if CLI_USE(HW_FLASH)
  cliAdd("flash", cliFlash);
#endif

  return true;
}

bool flashIsInit(void)
{
  return is_init;
}

//-- 주소 -> (뱅크, 뱅크상대 섹터번호)
//   ST 공식 예제(FLASH_EraseProgram)의 GetBank()/GetSector() 와 동일한 규칙.
//   SWAP_BANK 가 켜져 있으면 논리 뱅크가 뒤집힌다.
//
bool flashGetBankSector(uint32_t addr, uint32_t *p_bank, uint32_t *p_sector)
{
  uint32_t offset;

  if (addr < FLASH_BASE_ADDR || addr >= (FLASH_BASE_ADDR + FLASH_TOTAL_BYTES))
  {
    return false;
  }

  offset = addr - FLASH_BASE_ADDR;

  if (offset < FLASH_BANK_BYTES)
  {
    *p_bank = is_swap_bank ? FLASH_BANK_2 : FLASH_BANK_1;
  }
  else
  {
    *p_bank = is_swap_bank ? FLASH_BANK_1 : FLASH_BANK_2;
  }

  *p_sector = (offset % FLASH_BANK_BYTES) / FLASH_SECTOR_BYTES;

  return true;
}

bool flashInRange(uint32_t addr, uint32_t length)
{
  if (length == 0)
    return false;
  if (addr < FLASH_BASE_ADDR)
    return false;
  if ((uint64_t)addr + length > (uint64_t)FLASH_BASE_ADDR + FLASH_TOTAL_BYTES)
    return false;
  return true;
}

//-- 부트로더 자신을 지우거나 덮어쓰는 것을 막는 마지막 방어선.
//   오프바이원 하나가 그대로 벽돌로 이어지는 구간이다.
//
bool flashIsProtected(uint32_t addr, uint32_t length)
{
  uint32_t s = addr;
  uint32_t e = addr + length - 1;
  uint32_t bs = FLASH_ADDR_BOOT;
  uint32_t be = FLASH_ADDR_BOOT + FLASH_SIZE_BOOT - 1;

  if (e < bs || s > be)
    return false;

  return true;
}

void flashCacheInvalidate(void)
{
  __DSB();
  __ISB();

#ifdef HAL_ICACHE_MODULE_ENABLED
  // 부트로더는 ICACHE 를 켜지 않지만, 켠 채로 쓰는 프로젝트(앱)에서도
  // 이 파일을 그대로 쓰기 위해 방어적으로 무효화한다.
  if (READ_BIT(ICACHE->CR, ICACHE_CR_EN) != 0U)
  {
    HAL_ICACHE_Invalidate();
  }
#endif
}

//-- 대상이 지워진 상태(0xFF)인지 확인한다.
//
//   STM32H5 는 이미 프로그램된 쿼드워드를 다시 써도 HAL 이 에러를 돌려주지 않는다.
//   (실기 확인: HAL_FLASH_Program 이 HAL_OK 를 반환) 대신 ECC 가 깨져서, 그 워드를
//   나중에 읽는 순간 ECC 2비트 오류로 NMI 가 발생한다. 즉 하드웨어가 막아주지
//   않으므로 소프트웨어가 반드시 막아야 한다.
//
bool flashIsBlank(uint32_t addr, uint32_t length)
{
  const uint8_t *p = (const uint8_t *)addr;

  for (uint32_t i = 0; i < length; i++)
  {
    if (p[i] != 0xFF)
      return false;
  }
  return true;
}

bool flashErase(uint32_t addr, uint32_t length)
{
  bool     ret = true;
  uint32_t bank_s, sector_s;
  uint32_t bank_e, sector_e;

  if (!flashInRange(addr, length))
  {
    logPrintf("[E_] flashErase() range 0x%X %d\n", addr, length);
    return false;
  }
  if (flashIsProtected(addr, length))
  {
    logPrintf("[E_] flashErase() protected 0x%X\n", addr);
    return false;
  }

  if (!flashGetBankSector(addr, &bank_s, &sector_s))
    return false;
  if (!flashGetBankSector(addr + length - 1, &bank_e, &sector_e))
    return false;

  HAL_FLASH_Unlock();

  for (uint32_t bank = 0; bank < 2 && ret == true; bank++)
  {
    FLASH_EraseInitTypeDef init;
    uint32_t sector_err = 0;
    uint32_t first;
    uint32_t last;
    uint32_t bank_id;
    uint32_t bank_base = FLASH_BASE_ADDR + bank * FLASH_BANK_BYTES;
    uint32_t s;
    uint32_t e;

    // 요청 구간과 이 뱅크의 교집합을 구한다.
    s = (addr > bank_base) ? addr : bank_base;
    e = addr + length - 1;
    if (e > bank_base + FLASH_BANK_BYTES - 1)
      e = bank_base + FLASH_BANK_BYTES - 1;
    if (s > e)
      continue;

    if (!flashGetBankSector(s, &bank_id, &first))
      return false;
    if (!flashGetBankSector(e, &bank_id, &last))
      return false;

    // Banks 를 반드시 명시한다. 초기화하지 않으면 스택 쓰레기값에 따라
    // 엉뚱한 뱅크(=부트로더가 있는 뱅크1)가 지워질 수 있다.
    //
    init.TypeErase = FLASH_TYPEERASE_SECTORS;
    init.Banks     = bank_id;
    init.Sector    = first;
    init.NbSectors = last - first + 1;

    if (HAL_FLASHEx_Erase(&init, &sector_err) != HAL_OK)
    {
      logPrintf("[E_] Erase bank%d sec%d n%d err 0x%X sr 0x%X\n",
                (int)bank_id, (int)first, (int)(last-first+1),
                (unsigned int)HAL_FLASH_GetError(), (unsigned int)sector_err);
      ret = false;
    }
  }

  HAL_FLASH_Lock();
  flashCacheInvalidate();

  return ret;
}

bool flashWrite(uint32_t addr, uint8_t *p_data, uint32_t length)
{
  bool     ret = true;
  uint32_t buf32[FLASH_WRITE_SIZE/4] __attribute__((aligned(16)));
  uint8_t *buf = (uint8_t *)buf32;

  if (length == 0)
    return true;

  if (!flashInRange(addr, length))
  {
    logPrintf("[E_] flashWrite() range 0x%X %d\n", addr, length);
    return false;
  }
  if (flashIsProtected(addr, length))
  {
    logPrintf("[E_] flashWrite() protected 0x%X\n", addr);
    return false;
  }

  // STM32H5 는 이미 프로그램된 쿼드워드를 다시 쓸 수 없다(ECC).
  // 따라서 read-modify-write 를 하지 않고, 정렬되지 않은 시작 주소는 거부한다.
  // 호출부(boot/uf2)는 모두 16B 정렬을 보장한다.
  //
  if ((addr % FLASH_WRITE_SIZE) != 0)
  {
    logPrintf("[E_] flashWrite() align 0x%X\n", addr);
    return false;
  }

  HAL_FLASH_Unlock();

  for (uint32_t index = 0; index < length; index += FLASH_WRITE_SIZE)
  {
    uint32_t wr_len = length - index;

    if (wr_len > FLASH_WRITE_SIZE)
      wr_len = FLASH_WRITE_SIZE;

    // 꼬리 조각은 0xFF 로 채운다. 지운 직후 상태와 같아 ECC 상 안전하다.
    memset(buf, 0xFF, FLASH_WRITE_SIZE);
    memcpy(buf, &p_data[index], wr_len);

    // 재기록 금지. 하드웨어가 막아주지 않으므로 여기서 걸러야 한다.
    if (flashIsBlank(addr + index, FLASH_WRITE_SIZE) != true)
    {
      logPrintf("[E_] flashWrite() not blank 0x%X\n", (unsigned int)(addr + index));
      ret = false;
      break;
    }

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD,
                          addr + index, (uint32_t)buf) != HAL_OK)
    {
      logPrintf("[E_] Program 0x%X err 0x%X\n",
                (unsigned int)(addr + index), (unsigned int)HAL_FLASH_GetError());
      ret = false;
      break;
    }
  }

  HAL_FLASH_Lock();
  flashCacheInvalidate();

  return ret;
}

bool flashRead(uint32_t addr, uint8_t *p_data, uint32_t length)
{
  if (!flashInRange(addr, length))
    return false;

  memcpy(p_data, (const void *)addr, length);
  return true;
}


#if CLI_USE(HW_FLASH)
void cliFlash(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("flash init  : %d\n", is_init);
    cliPrintf("swap bank   : %d\n", is_swap_bank);
    cliPrintf("sector size : %d KB\n", (int)(FLASH_SECTOR_BYTES/1024));
    cliPrintf("bank size   : %d KB\n", (int)(FLASH_BANK_BYTES/1024));
    cliPrintf("\n");
    cliPrintf("BOOT   : 0x%08X %d KB\n", FLASH_ADDR_BOOT,   FLASH_SIZE_BOOT/1024);
    cliPrintf("FIRM   : 0x%08X %d KB\n", FLASH_ADDR_FIRM,   FLASH_SIZE_FIRM/1024);
    cliPrintf("SLOT0  : 0x%08X %d KB\n", FLASH_ADDR_SLOT0,  FLASH_SIZE_SLOT/1024);
    cliPrintf("SLOT1  : 0x%08X %d KB\n", FLASH_ADDR_SLOT1,  FLASH_SIZE_SLOT/1024);
    cliPrintf("BOOTLOG: 0x%08X %d KB\n", FLASH_ADDR_BOOT_LOG, FLASH_SIZE_BOOT_LOG/1024);
    cliPrintf("NVS    : 0x%08X %d KB\n", FLASH_ADDR_NVS,    FLASH_SIZE_NVS/1024);
    ret = true;
  }

  if (args->argc == 3 && args->isStr(0, "read"))
  {
    uint32_t addr   = (uint32_t)args->getData(1);
    uint32_t length = (uint32_t)args->getData(2);
    uint8_t  data;

    for (uint32_t i=0; i<length; i++)
    {
      if (flashRead(addr+i, &data, 1) == true)
      {
        if ((i % 16) == 0)
          cliPrintf("\n0x%08X : ", addr+i);
        cliPrintf("%02X ", data);
      }
      else
      {
        cliPrintf("\nreadFail : 0x%X\n", addr+i);
        break;
      }
    }
    cliPrintf("\n");
    ret = true;
  }

  if (args->argc == 3 && args->isStr(0, "erase"))
  {
    uint32_t addr   = (uint32_t)args->getData(1);
    uint32_t length = (uint32_t)args->getData(2);
    uint32_t pre_time;

    pre_time = millis();
    if (flashErase(addr, length) == true)
      cliPrintf("erase OK : %d ms\n", millis()-pre_time);
    else
      cliPrintf("erase Fail\n");
    ret = true;
  }

  if (args->argc == 3 && args->isStr(0, "write"))
  {
    uint32_t addr = (uint32_t)args->getData(1);
    uint32_t data = (uint32_t)args->getData(2);
    uint8_t  buf[16] __attribute__((aligned(16)));
    uint32_t pre_time;

    memset(buf, 0xFF, sizeof(buf));
    memcpy(buf, &data, 4);

    pre_time = millis();
    if (flashWrite(addr, buf, sizeof(buf)) == true)
      cliPrintf("write OK : %d ms\n", millis()-pre_time);
    else
      cliPrintf("write Fail\n");
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("flash info\n");
    cliPrintf("flash read  addr length\n");
    cliPrintf("flash erase addr length\n");
    cliPrintf("flash write addr data\n");
  }
}
#endif

#endif
