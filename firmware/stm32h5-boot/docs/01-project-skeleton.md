# 01. 프로젝트 골격

## 목적

`firmware/stm32h5-fw` 의 구조와 규약을 그대로 따르는 별도 CMake 프로젝트를 만들고,
128KB 안에 들어가는지 초기 크기를 실측한다.

## 대상 파일

- `CMakeLists.txt`, `tools/arm-none-eabi-gcc.cmake`
- `src/bsp/ldscript/STM32H563xx_BOOT.ld`
- `src/lib/ST/**` (CMSIS + HAL), `src/lib/tinyusb/**`
- `tools/uf2/{uf2conv.py, uf2families.json}`

## 무엇을 가져오고 무엇을 뺐나

앱에서 그대로 복사: `bsp/`, `common/`, `lib/ST/{CMSIS,STM32H5xx_HAL_Driver}`,
`hw/driver/{led,log,rtc,gpio,fault,assert,uart}.c`, `ap/modules/module.*`,
`ap/modules/common/cli/cli_mgr.*`

**제외**: `wiznet/`, `wiz_spi.c`, `event.c`, `osal/thread.c`, `cli_gui.c`,
`swtimer.c`, `cli/driver/cli_net.c`, `iperf.c`, `lib/ST/STM32_USB_Device_Library`

TinyUSB 0.18.0 은 `stm32h562rgt6-bd/firmware/stm32h562rg-tinyusb` 에서 가져왔다
(같은 H5 계열의 검증된 포팅). `uf2conv.py` 는 `convex/firmware/convex-boot` 에서 가져왔다.

## 설계 결정과 근거

### `-O0` 대신 `-Os`

앱은 `-O0` 에 더해 `set_source_files_properties(... "-O0")` 로 HAL 까지 `-O0` 로
빌드한다. 그 조합이면 HAL 만으로 70KB 를 넘길 수 있어 128KB 부트로더에 맞지 않는다.
부트로더는 `-Os` 전역으로 가고 해당 블록을 삭제했다.

### `LANGUAGES ASM C`

부트로더에 C++ 이 없으므로 `CXX` 를 빼고 `-lstdc++ -lsupc++` 도 제거했다.

### 링커스크립트

앱의 `STM32H563xx_FLASH.ld` 를 그대로 쓰되 `FLASH` 영역 길이만 바꿨다.
`VECTOR`/`VER`/`.module`/`.noinit` 구조는 앱과 동일하게 유지한다.

```
VECTOR (rx) : ORIGIN = 0x08000000, LENGTH = 1K
VER    (rx) : ORIGIN = 0x08000400, LENGTH = 1K
FLASH  (rx) : ORIGIN = 0x08000800, LENGTH = 128K-2K
```

## 함정 / 주의사항

### `-T` 가 빌드 디렉토리 기준 상대경로다

`-T../src/bsp/ldscript/STM32H563xx_BOOT.ld` 이므로 빌드 디렉토리는 반드시
소스 루트 바로 아래 `build/` 여야 한다. 앱과 동일한 관례다.

### `module.h` 가 `event_func_t` 에 의존했다

`module_t` 구조체에 `event_func_t event_cb` 필드가 있어 `event.h` 없이는 컴파일되지
않았다. 부트로더는 이벤트 시스템을 쓰지 않으므로 `#ifdef _USE_HW_EVENT` 로 감쌌다.
`module.c` 의 `eventSubFunc()` 호출도 같이 감쌌다. **앱/부트로더 공용 파일이 되었다.**

### `stm32h5xx_it.c` 의 `SysTick_Handler` 가 `swtimerISR()` 을 직접 불렀다

`swtimer.c` 를 빼면 링크 에러가 난다. `#ifdef _USE_HW_SWTIMER` 로 감쌌다.

### macOS 12 에서 STM32CubeCLT 1.22 가 실행되지 않는다

> Qt requires macOS 13.0.0 or later, you have macOS 12.7.6

**1.21.0 을 쓴다.** 1.16/1.18/1.21 은 정상 동작한다.

## 검증

```bash
cmake -S . -B build && cmake --build build -j8
```

## 실측 결과

첫 빌드(USB/플래시 드라이버 제외) 기준:

```
Memory region   Used Size   Region Size   %age Used
    NO_INIT:         76 B         8 KB      0.93%
        RAM:       8688 B       632 KB      1.34%
     VECTOR:        588 B         1 KB     57.42%
      FLASH:      49752 B       126 KB     38.56%
```

섹션 배치도 의도대로 나왔다.

```
.isr_vector  VMA 08000000
.version     VMA 08000400     <- firm_ver
.text        VMA 08000800
_smodule/_emodule 간격 0x60 = module_t(48B) x 2 (ap, cli)
.noinit      VMA 20000000
```

계획 단계의 추정치(87~112KB)보다 훨씬 여유가 있다. TinyUSB 3종 클래스와
uf2/boot/cmd 를 더해도 126KB 안에 충분히 들어갈 전망이다.
