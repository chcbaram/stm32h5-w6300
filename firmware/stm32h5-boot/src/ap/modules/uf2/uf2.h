#ifndef UF2_H_
#define UF2_H_


#include "uf2_def.h"


// All entries are little endian.
#define UF2_MAGIC_START0  0x0A324655UL    // "UF2\n"
#define UF2_MAGIC_START1  0x9E5D5157UL    // Randomly selected
#define UF2_MAGIC_END     0x0AB16F30UL

#define UF2_FLAG_NOFLASH  0x00000001
#define UF2_FLAG_FAMILYID 0x00002000

#define UF2_RET_NOT_UF2   (-1)            // UF2 블록이 아님 (FAT/디렉터리 기록)
#define UF2_RET_ERR       (-2)            // 플래시 기록 실패


typedef struct
{
  uint32_t numBlocks;
  uint32_t numWritten;
  bool     aborted;
  uint8_t  writtenMask[MAX_BLOCKS / 8 + 1];
} WriteState;

typedef struct
{
  uint32_t magicStart0;
  uint32_t magicStart1;
  uint32_t flags;
  uint32_t targetAddr;
  uint32_t payloadSize;
  uint32_t blockNo;
  uint32_t numBlocks;
  uint32_t familyID;
  uint8_t  data[476];
  uint32_t magicEnd;
} UF2_Block;


bool     uf2Init(void);
void     uf2Update(void);
void     uf2RequestJump(void);
bool     uf2IsBusy(void);
uint8_t  uf2GetPercent(void);

void     uf2DiskSetMedium(bool enable);
bool     uf2DiskGetMedium(void);

int      uf2WriteBlock(uint32_t block_no, uint8_t *data, WriteState *state);
bool     uf2FlashComplete(WriteState *state);


#endif
