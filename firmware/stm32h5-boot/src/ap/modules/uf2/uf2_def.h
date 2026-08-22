#ifndef UF2_DEF_H_
#define UF2_DEF_H_


#include "ap_def.h"


#define UF2_PRODUCT_NAME          _DEF_BOARD_NAME
#define UF2_BOARD_ID              "STM32H5-W6300"
#define UF2_VOLUME_LABEL          "H5BOOT"


//-- 두 상수를 반드시 분리한다.
//
//   UF2_MAX_FW_SIZE   : 펌웨어 크기 상한. writtenMask/경계 검사 기준.
//   UF2_DISK_BLOCK_NUM: FAT16 지오메트리 기준. 손으로 만든 부트섹터가
//                       총섹터 0x8000, FAT당 128섹터로 하드코딩되어 있으므로
//                       이 값을 펌웨어 크기에 연동시키면 호스트가 디스크를 못 읽는다.
//
#define UF2_MAX_FW_SIZE           (FLASH_SIZE_FIRM - FLASH_SIZE_TAG)   // 447KB

#define UF2_DISK_BLOCK_NUM        32768        // 16MB
#define UF2_DISK_BLOCK_SIZE       512

#define UF2_PAYLOAD_SIZE          256
#define MAX_BLOCKS                (UF2_MAX_FW_SIZE / UF2_PAYLOAD_SIZE + 100)

#define UF2_ERASE_SECTOR_SIZE     FLASH_SECTOR_SIZE                    // 8KB
#define UF2_ERASE_SECTOR_MAX      (FLASH_SIZE_SLOT / UF2_ERASE_SECTOR_SIZE)


//-- 상태머신 대기 시간
//
#define UF2_COMPLETE_WAIT_MS      1000     // 호스트의 FAT/디렉터리 기록 + SYNC 완료 대기
#define UF2_JUMP_WAIT_MS          300


#endif
