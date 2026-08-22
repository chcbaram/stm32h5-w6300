# 12. 앱(stm32h5-fw) 변경 사항

## 목적

부트로더 뒤로 옮기고, 부트로더와 코드를 공유하며, 부팅 성공을 확정한다.

## 필수 변경 (없으면 오동작)

| 파일 | 변경 | 안 하면 |
|---|---|---|
| `src/bsp/ldscript/STM32H563xx_FLASH.ld` | `VECTOR 0x08020400`, `VER 0x08020800`, `FLASH 0x08020C00 (448K-3K)` | 부트로더가 못 찾는다 |
| `src/bsp/device/system_stm32h5xx.c` | `SCB->VTOR = (uint32_t)&_fw_flash_begin;` | 인터럽트가 부트로더 벡터로 간다 |
| `src/hw/hw_def.h` | `HW_RESET_BOOT` **1 → 0** | 앱이 이미 클리어된 RCC 플래그를 읽어 항상 "원인 없음" |
| `src/ap/ap.c` | `HW_BOOT_CONFIRM_MS` 뒤 `resetConfirmBoot()` | **3회 부팅마다 오탐 롤백** (실기 재현됨) |

**TAG(0x08020000, 1KB)는 링커 영역에서 제외한다.** 부트로더가 기록하므로 앱 bin 에
들어가면 안 되고, 그래야 UF2 `targetAddr = 0` ↔ `슬롯 베이스 + FLASH_SIZE_TAG`
매핑이 성립한다.

## 부트로더와 공용인 파일

아래는 **두 프로젝트에 같은 내용**을 둔다. 한쪽을 고치면 반대쪽에 복사한다.

```
src/common/hw/include/{reset.h, fault.h, cmd.h}
src/hw/driver/{reset.c, fault.c, flash.c, cmd.c}
src/ap/modules/boot/{boot.c, boot.h, boot_log.c, boot_log.h}
src/ap/modules/common/cli/cli_mgr.c
src/ap/modules/cmd/{cmd_task.c, cmd_task.h}
src/ap/modules/cmd/driver/{drv_usb.c, drv_cli.c}
src/ap/modules/cmd/process/cmd_boot.c
```

**`drv_hid.c` 만 예외다.** 부트로더는 TinyUSB, 앱은 ST 스택을 부른다.
리포트 규약(선두 바이트 = 유효 길이)은 동일하다.

채널 드라이버는 `drv_` 접두어로 통일했다 — 파일명·함수명·변수명 모두.
`drv_usb_driver`/`drvUsb*()`, `drv_hid_driver`/`drvHid*()`, `drvCli*()`.

동작 차이는 매크로로만 갈린다.

- `HW_RESET_BOOT` — 0 이면 더블클릭 판정 코드가 통째로 빠진다
- `FLASH_PROTECT_ADDR/SIZE` — 부트로더는 자기 자신만, **앱은 뱅크1 전체**를 보호한다
  (앱은 뱅크2 슬롯에만 쓰면 된다)
- `HW_DEV_MODE` — `BOOT_CMD_INFO` 응답에 실려 호스트가 어느 쪽인지 판별한다

## hw_def.h 에 추가한 것

플래시 배치(`FLASH_ADDR_*`/`FLASH_SIZE_*`)와 백업 레지스터 배정을 **부트로더와
동일하게** 둔다. 어긋나면 앱이 부팅하지 않는다.

```c
#define HW_RTC_BOOT_MODE   RTC_BKP_DR3
#define HW_RTC_RESET_BITS  RTC_BKP_DR4
#define HW_RTC_RESET_CNT   RTC_BKP_DR5
#define HW_RTC_BOOT_TRY    RTC_BKP_DR6
#define HW_RTC_FAULT_CNT   RTC_BKP_DR7
#define HW_RTC_ECC_ADDR    RTC_BKP_DR8
```

## .non_cache 제거

`uart.c` 의 `uart_tbl` 이 `__attribute__((section(".non_cache")))` 였는데
**링커스크립트에 그 섹션 정의가 없다.** orphan 으로 `_edata` 와 `_sbss` 사이에
놓여서 startup 의 `.data` 복사와 `.bss` 클리어 **어느 쪽에도 포함되지 않는다.**

즉 부팅 시 SRAM 쓰레기값이고, LMA 가 플래시에 잡혀 2136바이트도 낭비하고 있었다.
부트로더와 같이 속성을 제거했다.

## 빌드 후처리

빌드하면 `.uf2` 가 함께 나온다.

```cmake
uf2conv.py ${PROJECT_NAME}.bin --base 0x0 --family 0xFFFF0003 --convert
```

**`--base 0x0` 를 반드시 명시한다.** `uf2conv.py` 의 기본 `appstartaddr` 는 `0x2000`
이라 생략하면 8KB 오프셋 구멍이 생긴다.

## 호스트 도구

`tools/download/` 에 cmd 프로토콜 다운로더를 두었다. VSCode 태스크에 연결되어 있다.

```bash
python3 tools/download/download.py            # CDC
python3 tools/download/download.py --hid      # HID
```

## USB composite (CDC + HID)

앱도 부트로더와 같은 VID/PID 로 열거되고 같은 커맨드 셋에 답한다.
설계와 함정은 `15-app-hid.md` 에 따로 정리했다. 요약만 적으면:

| 항목 | 값 |
|---|---|
| 인터페이스 | ITF0/1 CDC(IAD) · ITF2 HID(vendor 0xFF00) |
| 엔드포인트 | CDC 0x81/0x01/0x82 · HID 0x83/0x03 |
| PMA | 464B / 2048B |
| 구성 디스크립터 | 107B |

`usbd_conf.h` 에 다음을 명시해야 한다. 빠뜨리면 조용히 오동작한다.

```c
#define USE_USBD_COMPOSITE
#define USBD_COMPOSITE_USE_IAD   1     // 기본값이 없다. 빼면 윈도우 VCP 실패
#define USBD_MAX_NUM_INTERFACES  3
#define USBD_MAX_SUPPORTED_CLASS 2U
```

## 실측 결과

```
FLASH 165,088 B / 445 KB (36.2%)
```

UF2 로 갱신 후 앱이 정상 부팅하고 USB CDC · W6300 이더넷(DHCP/SNTP)까지 동작한다.
10초 뒤 `[OK] resetConfirmBoot()` 가 찍히고 `boot try : 0 / fault count : 0` 으로
유지된다.
