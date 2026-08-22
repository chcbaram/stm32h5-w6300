# 03. 플래시 드라이버 (STM32H5)

## 목적

`flash.h` 의 기존 인터페이스(`flashInit/Erase/Write/Read`)를 STM32H5 내부 플래시로
구현한다. 부트로더에서 가장 위험한 부분이며, 오프바이원 하나가 그대로 벽돌로 이어진다.

## 대상 파일

- `src/hw/driver/flash.c`
- `src/bsp/device/stm32h5xx_it.c` (`NMI_Handler`)

레퍼런스는 ST 공식 예제
`STM32Cube_FW_H5_V1.5.1/Projects/NUCLEO-H563ZI/Examples/FLASH/FLASH_EraseProgram/Src/main.c`
의 `GetSector()` / `GetBank()` / 프로그램 루프다.

## STM32H5 플래시 특성

| 항목 | 값 |
|---|---|
| 전체 | 2MB = 뱅크1 1MB + 뱅크2 1MB |
| 섹터 | 8KB (`FLASH_SECTOR_SIZE = 0x2000`), 뱅크당 128개 |
| 프로그램 단위 | **16바이트 쿼드워드** (`FLASH_TYPEPROGRAM_QUADWORD`) |
| ECC | 쿼드워드 단위. 지워진 상태(0xFF)는 유효 ECC |
| RWW | **뱅크 간에만** 유효 |

## 함정 전수

### 1. `EraseInitStruct.Banks` 를 반드시 명시 대입한다

`HAL_FLASHEx_Erase` 는 `Banks` 로 `FLASH_CR.BKSEL` 을 정하고, `Sector` 는
**뱅크 상대 인덱스(0–127)** 다. 즉 뱅크2 의 첫 섹터는 "섹터 128"이 아니라
`Banks=FLASH_BANK_2, Sector=0` 이다.

초기화를 빠뜨리면 스택 쓰레기값에 따라 **뱅크1(부트로더가 있는 곳)이 지워질 수 있다.**
참고로 `baram-qmk-h7s-boot/src/hw/driver/flash.c` 는 이 필드를 초기화하지 않는다.

### 2. `FLASH_BANK_SIZE` 매크로를 상수식에 쓰면 안 된다

```c
#define FLASH_SIZE       ((*(uint16_t *)FLASHSIZE_BASE) << 10)   // 런타임 레지스터 읽기
#define FLASH_BANK_SIZE  (FLASH_SIZE >> 1)
```

배열 크기나 `#if` 에 쓰면 컴파일 에러 또는 조용한 오동작이 된다. 리터럴 `0x100000`
을 쓴다. (`bsp.c` 의 `mpuInit()` 이 `0x08FFF800` 을 보호하는 이유가 바로 이
`FLASHSIZE_BASE` 읽기다.)

### 3. read-modify-write 금지 — **하드웨어가 막아주지 않는다**

이미 프로그램된 쿼드워드를 다시 쓰면 ECC 가 깨진다. 문제는 **`HAL_FLASH_Program` 이
`HAL_OK` 를 반환한다**는 점이다(실기 확인). 즉 하드웨어/HAL 이 걸러주지 않으므로
소프트웨어가 반드시 막아야 한다.

```c
// 재기록 금지. 하드웨어가 막아주지 않으므로 여기서 걸러야 한다.
if (flashIsBlank(addr + index, FLASH_WRITE_SIZE) != true)
{
  logPrintf("[E_] flashWrite() not blank 0x%X\n", addr + index);
  ret = false; break;
}
```

시작 주소가 16B 정렬이 아니면 그냥 실패시킨다. 호출부(boot/uf2)는 모두 16B 정렬을
보장하도록 설계했다.

### 4. 정렬된 로컬 버퍼를 쓴다

`FLASH_Program_QuadWord()` 는 dest/src 를 `uint32_t*` 로 4회 복사한다. dest 는 16B
정렬이 하드웨어 요구사항이고 src 도 최소 4B 정렬이 필요하다. UF2 블록 데이터는
512B 버퍼의 오프셋 32 라서 16B 정렬이 아니므로, **호출자 포인터를 그대로 넘기면 안 된다.**

```c
uint32_t buf32[FLASH_WRITE_SIZE/4] __attribute__((aligned(16)));
memset(buf, 0xFF, FLASH_WRITE_SIZE);   // 꼬리 패딩은 0xFF
memcpy(buf, &p_data[index], wr_len);
```

### 5. SWAP_BANK

ST 의 `GetBank()` 가 `FLASH->OPTSR_CUR` 의 `SWAP_BANK`(bit31)로 뱅크를 반전시킨다.
동일하게 구현했다. 부트로더는 옵션바이트를 **쓰지 않는다**(`HAL_FLASHEx_OBProgram`
호출 금지). 현재 보드는 `SWAP_BANK = 0`.

### 6. 자기 보호

```c
static bool flashIsProtected(uint32_t addr, uint32_t length)  // [BOOT, BOOT+128KB) 와 겹치면 true
```

`flashErase` / `flashWrite` 양쪽 진입부에서 검사한다. 오프바이원 하나가 벽돌로
이어지는 유일한 방어선이다.

### 7. 뱅크1 실행 중 뱅크1 소거/기록

RWW 는 뱅크 간에만 유효하므로, 뱅크1 에서 실행하면서 뱅크1 을 지우면 **AHB 스톨**이
발생한다(폴트가 아니다). 기능적으로는 동작하지만 그 구간에 인터럽트가 서비스되지
않는다. 따라서 `bootApplySlot()` 직전에 `usbDisconnect()` 를 호출하고, **IWDG 는
쓰지 않는다.**

### 8. ECC 2비트 오류 → NMI — 가장 중요

가장 위험한 발견이다. 기존 `stm32h5xx_it.c` 의 `NMI_Handler` 는 `while(1)` 무한루프였다.
STM32H5 는 플래시 ECC 2비트 오류를 NMI 로 올리므로, 반쯤 기록된 쿼드워드를 읽는
순간 **부트로더가 영구 정지**한다. 복구 수단이 복구 불가가 되는 최악의 실패 모드다.

절대 멈추지 않도록 교체했다.

```c
void NMI_Handler(void)
{
  if (FLASH->ECCDETR & FLASH_ECCR_ECCD)          // ECC 2비트 검출
  {
    uint32_t addr_ecc = FLASH->ECCDETR & FLASH_ECCR_ADDR_ECC;   // [15:0]
    uint32_t bank     = (FLASH->ECCDETR & FLASH_ECCR_BK_ECC) ? 1 : 0;   // bit22

    FLASH->ECCDETR |= FLASH_ECCR_ECCD;           // rc_w1 클리어
    rtcSetReg(HW_RTC_ECC_ADDR, HW_ECC_MAGIC | (bank << 20) | addr_ecc);
  }
  NVIC_SystemReset();
}
```

다음 부팅에 `bootUp()` 이 `HW_RTC_ECC_ADDR` 를 보고 해당 영역을 정리한다(9단계).

비트 위치: `ECCD = bit31`, `ECCC = bit30`, `BK_ECC = bit22`, `ADDR_ECC = [15:0]`.
레지스터는 `FLASH->ECCDETR`(0x40022104), `FLASH->ECCCORR`(0x40022100).

## 검증

부트로더 CLI 로 **뱅크2에서 먼저** 검증하고, 그다음 뱅크1(FIRM 영역, 현재 앱 없음)에서
검증했다.

## 실측 결과

| 시험 | 결과 |
|---|---|
| 뱅크2 소거 8KB | OK, **2 ms** |
| 뱅크2 기록/읽기 | OK (`78 56 34 12 FF..`) |
| 뱅크1 소거 8KB (실행 중 뱅크) | OK, **1 ms** — AHB 스톨 정상 |
| 뱅크1 기록/읽기 | OK (`BE BA FE CA`) |
| 뱅크1 64KB(8섹터) 연속 소거 | OK, **8 ms** |
| 부트 영역 소거 `flash erase 0x08000000 8192` | **거부** `flashErase() protected` |
| 부트 영역 기록 `flash write 0x08010000 ...` | **거부** `flashWrite() protected` |
| 비정렬 기록 `flash write 0x08100004 ...` | **거부** `flashWrite() align` |
| 쿼드워드 재기록 (blank check 전) | `write OK` 반환 후 **읽는 순간 NMI 정지** |
| 쿼드워드 재기록 (blank check 후) | **거부** `flashWrite() not blank`, 원본 유지 |

### ECC/NMI 복구 실측

SWD 로 이미 기록된 `0x08100000` 에 직접 재기록해 ECC 를 파괴했다.

```
주입 전 : 12345678 FFFFFFFF
주입 후 : 12241668 00000000     <- 부분 기록, ECC 파괴
```

이 상태에서 부트로더가 해당 주소를 읽자:

- `flash read 0x08100000 16` → 출력 도중 **즉시 재부팅**(부트 배너 재출력)
- 코어는 `cliMain()` 에서 정상 동작 — NMI 에 갇히지 않음
- `TAMP->BKP8R = 0xEC100000`
  - `0xEC` = `HW_ECC_MAGIC`
  - bit20 = 1 → 뱅크2
  - `ADDR_ECC` = 0 → 오프셋 0
  - → 정확히 `0x08100000` 을 지목

### 참고: 백업 레지스터 물리 주소

`TAMP_BASE = 0x44007C00`, `BKPxR = TAMP_BASE + 0x100 + x*4`

| 레지스터 | 주소 | 용도 |
|---|---|---|
| BKP3R | `0x44007D0C` | `HW_RTC_BOOT_MODE` |
| BKP4R | `0x44007D10` | `HW_RTC_RESET_BITS` |
| BKP5R | `0x44007D14` | `HW_RTC_RESET_CNT` |
| BKP6R | `0x44007D18` | `HW_RTC_BOOT_TRY` |
| BKP7R | `0x44007D1C` | `HW_RTC_FAULT_CNT` |
| BKP8R | `0x44007D20` | `HW_RTC_ECC_ADDR` |
