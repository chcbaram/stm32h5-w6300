#include "test_common.h"

int g_pass = 0, g_fail = 0;

static void fillImage(uint8_t *buf, uint32_t size, uint8_t seed)
{
  for (uint32_t i = 0; i < size; i++)
    buf[i] = (uint8_t)(seed + (i * 7) + (i >> 8));
}

uint16_t testImageCrc(uint32_t size, uint8_t seed)
{
  static uint8_t buf[512*1024];
  fillImage(buf, size, seed);
  return utilCalcCRC(0, buf, size);
}

//-- 부트로더가 만드는 것과 동일한 레이아웃을 목 플래시에 직접 찍는다.
//   (uf2FlashFlush() 와 같은 순서/내용)
//
void testMakeImage(uint32_t base, uint32_t size, uint32_t seq, uint8_t seed)
{
  static uint8_t buf[512*1024];
  firm_tag_t  tag;
  boot_slot_t slot;
  uint8_t    *p = mockFlashPtr(base);

  memset(p, 0xFF, FLASH_SIZE_TAG + size);

  fillImage(buf, size, seed);
  memcpy(p + FLASH_SIZE_TAG, buf, size);

  tag.magic_number = TAG_MAGIC_NUMBER;
  tag.fw_addr      = FLASH_SIZE_TAG;
  tag.fw_size      = size;
  tag.fw_crc       = utilCalcCRC(0, buf, size);
  tag.tag_crc      = utilCalcCRC(0, (uint8_t *)&tag, 16);
  memcpy(p, &tag, sizeof(tag));

  slot.magic = BOOT_SLOT_MAGIC;
  slot.seq   = seq;
  slot.flags = BOOT_SLOT_FLAG_NONE;
  slot.crc   = utilCalcCRC(0, (uint8_t *)&slot, 12);
  memcpy(p + BOOT_SLOT_TAG_OFFSET, &slot, sizeof(slot));
}
