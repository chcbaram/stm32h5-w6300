# 04. 리셋 더블클릭 감지

## 목적

리셋 버튼을 빠르게 두 번 누르면 부트로더에 머무르게 한다. 앱이 완전히 깨져도
동작해야 하므로 앱의 협조에 의존하지 않는다.

## 대상 파일

- `src/hw/driver/reset.c`
- `src/common/hw/include/reset.h` (API 추가)
- `src/hw/hw_def.h` (백업 레지스터 배정, 파라미터)

## 백업 레지스터 배정

`TAMP->BKPxR` 을 쓴다. `HAL_RTCEx_BKUPWrite` 는 사실상 `TAMP->BKPxR` 직접 쓰기라
LSE 발진 없이도 동작하며, `SystemClock_Config()` 가 이미 `HAL_PWR_EnableBkUpAccess()`
를 호출한다.

| 매크로 | 레지스터 | 물리 주소 | 용도 |
|---|---|---|---|
| `HW_RTC_BOOT_MODE` | BKP3R | `0x44007D0C` | `MODE_BIT_BOOT` / `MODE_BIT_UPDATE` |
| `HW_RTC_RESET_BITS` | BKP4R | `0x44007D10` | 리셋 원인 비트 |
| `HW_RTC_RESET_CNT` | BKP5R | `0x44007D14` | 더블클릭 카운트 |
| `HW_RTC_BOOT_TRY` | BKP6R | `0x44007D18` | 부팅 확인 카운터 |
| `HW_RTC_FAULT_CNT` | BKP7R | `0x44007D1C` | 폴트 리셋 카운터 |
| `HW_RTC_ECC_ADDR` | BKP8R | `0x44007D20` | ECC 2비트 오류 위치 |

모든 카운터는 상위 16비트에 매직(`0xA55A`)을 넣어 유효성을 판정한다. 보드에 코인셀이
없어 `VBAT = VDD` 이므로 전원이 끊기면 백업 도메인이 날아가 부정값이 되기 때문이다.

## 판정 로직

```c
cnt = resetCntLoad();

if      (reset_bits & (1<<RESET_BIT_POWER))                       cnt = 0;
else if (reset_bits & ((1<<RESET_BIT_SOFT)|(1<<RESET_BIT_WDG)))   cnt = 0;
else if (reset_bits & (1<<RESET_BIT_PIN))                         cnt++;
else                                                              cnt = 0;

reset_count = cnt;

if (cnt == 1)                       // 두 번째 클릭을 받는 창
{
  resetCntSave(cnt);
  ledOn(_DEF_LED1);
  HAL_Delay(HW_RESET_DBLCLK_MS);    // 300ms
  ledOff(_DEF_LED1);
}
resetCntSave(0);
```

## 설계 결정과 근거

### 판정 순서 — POWER → SOFT/WDG → PIN

**실기에서 확인한 중요한 사실: STM32H5 는 소프트 리셋(`NVIC_SystemReset`)에서도
`PINRSTF` 가 함께 세트된다.** 내부 리셋이 NRST 핀으로 전파되어 되먹임되기 때문이다.

따라서 SOFT/WDG 를 PIN 보다 **먼저** 걸러야 한다. 그러지 않으면 `resetToBoot()`,
`resetToUpdate()`, 폴트 리셋마다 더블클릭 카운트가 올라가 부트로더로 오진입한다.

POWER(BOR)를 가장 먼저 보는 이유도 같다. 전원 인가 시 `BORRSTF` 와 `PINRSTF` 가
동시에 세트되므로, 순서를 뒤집으면 "전원 껐다 켜기 2회" 로도 부트로더에 들어간다.

> H5 RCC 에는 `RCC_FLAG_PWRRST` 가 없다. PIN/BOR/SFT/IWDG/WWDG/LPWR 6종뿐이다.

### 대기 시간 300ms

`rp2040_boot` / `ch32v305_06_boot` 등 최신 프로젝트의 `RESET_COUNT_DELAY` 와 같은
값이다(구형 `stm32f103_boot` 은 500ms). 실제 더블탭 간격은 150~350ms 에 몰려 있어
300ms 면 충분하고, 500ms 는 매 NRST 마다 붙는 지연이 불필요하게 길다.

**이 지연은 NRST 버튼을 누른 경우에만 발생한다.** 전원 인가 부팅은 POR 로 판정되어
`cnt = 0` 이므로 대기 창에 들어가지 않는다. 제품 사용 시 지배적인 경로에는 영향이 없다.

### `delay()` 가 아니라 `HAL_Delay()`

`bsp.c` 의 `delay()` 는 `cliLoopIdle()` 을 호출한다. `resetInit()` 시점에는 USB 도
모듈도 초기화 전이므로 재진입하면 안 된다.

### `resetCntLoad()` / `resetCntSave()` 분리

RTC/LSE 에 문제가 생기면 `.noinit` SRAM 매직 방식으로 즉시 바꿔 끼울 수 있도록
저장소 접근을 두 함수로 격리했다. 링커스크립트에 `NO_INIT` 영역(0x20000000, 8KB)이
이미 있다.

## 함정 / 주의사항

### ST-LINK 로는 순수 NRST 를 재현할 수 없다

`STM32_Programmer_CLI` 는 `mode=UR` 이든 `-rst` 든 **항상 소프트 리셋을 동반**해서
`PIN + SOFT` 가 같이 세트된다. 따라서 SOFT 우선 규칙에 걸려 카운트가 0 이 된다.
**물리 버튼을 눌러야만 검증된다.**

### `reset_count` 출력 시점

`reset_count` 는 300ms 대기 **뒤에** 로그로 나온다. 대기 도중 리셋된 부팅은
배너만 출력하고 `reset_count` 는 출력하지 않는다. 따라서 로그를 셀 때
**배너 개수와 `reset_count` 개수를 따로 세야** 실제 리셋 횟수를 알 수 있다.
`test/target/monitor.py` 가 둘을 나눠 센다.

### 기존 코드의 배열 범위 초과

원본 `reset.c` 는 `reset_bit_str[]`(4개) / `mode_bit_str[]`(2개)를 모두
`RESET_BIT_MAX`(5)까지 순회했다. 배열 크기를 `[RESET_BIT_MAX]` / `[MODE_BIT_MAX]`
로 명시하고 루프 상한도 각각 맞췄다.

## 검증

```bash
cd firmware/stm32h5-boot/test/target && python3 monitor.py 20
```

## 실측 결과

| 케이스 | 기대 | 실측 |
|---|---|---|
| 소프트 리셋 (`mode=HOTPLUG -rst`) 연속 3회 | 0 | **0, 0, 0** |
| 물리 NRST 단발 | 1 | **배너 1회, `reset_count : 1`, `bits=['RESET_BIT_PIN']`** |
| 물리 NRST 더블클릭 | 2 | **배너 2회, `reset_count : 2`** |

더블클릭 로그:

```
[ 1.6s] BOOT #1                        <- 첫 리셋, 300ms 대기 진입(count 미출력)
[ 2.0s] BOOT #2                        <- 0.4초 후 두 번째 리셋 = 창 안
[ 2.0s]   -> reset_count : 2  bits=['RESET_BIT_PIN']
```

물리 버튼은 `RESET_BIT_PIN` 만 세트된다(SOFT 없음). SOFT 우선 판정이 실제 버튼에
대해 올바르게 동작함을 확인했다.
