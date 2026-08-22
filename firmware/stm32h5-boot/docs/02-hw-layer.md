# 02. hw 계층

## 목적

부트로더에 필요한 최소 드라이버만 올리고, `bsp → hw → ap` 호출 순서 규약을 지킨 채
USART1 로그와 CLI 가 동작하는 것을 실기에서 확인한다.

## 대상 파일

- `src/hw/hw_def.h`, `src/hw/hw.c`, `src/hw/hw.h`
- `src/hw/driver/{led,log,rtc,gpio,fault,assert,uart}.c`
- `src/bsp/bsp.c` (캐시 비활성, `bspDeInit()` 추가)
- `src/ap/ap.c`, `src/ap/modules/common/cli/cli_mgr.c`

## hwInit() 호출 순서

```c
cliInit();  logInit();  ledInit();  uartInit();
uartOpen(HW_UART_CH_SWD, 115200);
logOpen(HW_LOG_CH, 115200);   // 배너 출력
rtcInit();                    // resetInit 이 백업 레지스터를 쓰므로 먼저
resetInit();
faultInit();  assertInit();  gpioInit();  flashInit();
```

앱과 달리 `usbInit()` 은 `hwInit()` 이 아니라 `apInit()` 에서 `bootUp()` **뒤에**
호출한다. 앱으로 점프하는 경우 USB 열거를 아예 시작하지 않기 위해서다.
호스트에 장치가 나타났다 곧바로 사라지는 것을 막는다.

```c
void apInit(void)  { bootUp(); usbInit(); moduleInit(); }
void apMain(void)  { while(1) moduleUpdate(); }
void cliLoopIdle(void) { cliMgrEnable(false); moduleUpdate(); cliMgrEnable(true); }
```

## 설계 결정과 근거

### `module.c` / `cli_mgr` 를 유지한 이유

참고한 다른 부트로더들(convex, baram-45k-h7s)은 모듈 시스템 없이 단순 루프를 쓴다.
그러나 이 프로젝트는 둘 다 유지했다.

- `cliMgrEnable(false/true)` — `bsp.c` 의 `delay()` 가 `cliLoopIdle()` 을 부르는 구조라,
  블로킹 구간에서도 `usbUpdate()/uf2Update()` 가 돌면서 `cliMain()` 만 재진입을 막아야
  한다. 직접 재구현하면 미묘한 재진입 버그가 나기 쉬운 지점이다.
- 포트 자동 전환 — 앱의 `NET ↔ CLI` 전환과 부트로더의 `USB CDC ↔ USART1` 전환이
  같은 패턴이다.

`cli_mgr.c` 는 네트워크 분기를 `#ifdef _USE_HW_WIZNET`, CDC 분기를 `#ifdef _USE_HW_CDC`
로 감싸 **앱/부트로더 공용 파일**로 만들었다.

### 부트로더는 ICACHE/DCACHE 를 켜지 않는다

128KB 부트로더에 성능이 필요 없고, 플래시 소거/기록 후 캐시 무효화를 빠뜨려 옛
데이터를 읽는 버그 클래스를 통째로 없앨 수 있다. ST 공식 예제(`FLASH_EraseProgram`)도
플래시 조작 전후로 `HAL_ICACHE_Disable()` / `Enable()` 을 호출한다.

`mpuInit()` 은 유지했다. `0x08FFF800`(UID/FLASHSIZE) 영역을 non-cacheable RO 로
잡아두는 설정이며, 앱과 동일하게 두는 편이 안전하다.

## 함정 / 주의사항

### `.non_cache` 섹션이 실제 버그였다 (앱에도 존재)

`uart.c` 의 `uart_tbl` 이 `__attribute__((section(".non_cache")))` 로 선언되어 있는데,
**링커스크립트에 `.non_cache` 정의가 없다.** 그 결과 orphan 으로 배치되어:

```
_edata = 0x20002260
.non_cache  0x20002260 ~ 0x20002AB8   (2136 B)   <- 여기
_sbss  = 0x20002AB8
```

startup 의 `.data` 복사(`_sdata.._edata`)에도, `.bss` 클리어(`_sbss.._ebss`)에도
포함되지 않는다. 즉 **`uart_tbl` 이 부팅 시 SRAM 쓰레기값**이고, 동시에 LMA 가 플래시에
잡혀 **2136 바이트를 낭비**하고 있었다.

부트로더는 DCACHE 를 켜지 않으므로 속성 자체가 불필요하다. 제거했더니 `.bss` 가
2136B 늘고 플래시가 정확히 2136B 줄었다(51,888 → 49,752).

> **앱(`stm32h5-fw`)에도 같은 문제가 그대로 있다.** 12단계에서 함께 정리한다.

### `uart.c` 의 CDC 의존

`uart.c` 는 이미 `#if HW_USE_CDC == 1` 로 감싸져 있어 구조적 문제는 없었다.
`uart_hw_tbl` 의 항목 수만 3(SWD/USB/NET) → 2(SWD/USB)로 줄였다.

### CLI 개행은 CR(0x0D)

`cli.c` 의 `CLI_KEY_ENTER` 가 `0x0D` 다. 호스트에서 `\n` 을 보내면 에코만 되고
명령이 실행되지 않는다. 테스트 헬퍼(`test/target/lib/cli.py`)는 `\r` 을 보낸다.

## 검증

ST-LINK VCP(`/dev/cu.usbmodem1412102`, 115200 8N1)로 접속해 확인했다.

## 실측 결과

```
[ Bootloader Begin... ]
Booting..Name  : STM32H5-W6300-BOOT
Booting..Ver   : V260822R1
Booting..Clock : 250 Mhz
Booting..Addr  : 0x8000000

[OK] rtcInit()
[OK] resetInit()
     RESET_BIT_PIN
     RESET_BIT_SOFT
[OK] flashInit()
     Sector : 8 KB x 128 x 2bank
Boot Mode..
[  ] moduleInit()
       count : 2
[  ] moduleBegin()
cli#        cli OK
```

`help` 응답: `HELP MD LOG UART RTC RESET GPIO MODULE FLASH`

SWD 로 PC 를 샘플링하면 `moduleUpdate()` → `updateLed()` / `cliMain()` /
`uartAvailable()` 사이를 돌고 있어 메인 루프가 정상임을 확인했다.
