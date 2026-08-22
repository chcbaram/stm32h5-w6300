# 05. 펌웨어 검증과 점프

## 목적

FIRM 영역의 이미지를 검증하고 앱으로 점프한다. 슬롯 관리(핑퐁)의 토대가 되는
`bootVerifySlot()` / `bootApplySlot()` / `bootJumpFirm()` 을 함께 구현한다.

## 대상 파일

- `src/ap/modules/boot/{boot.c, boot.h}`
- `src/ap/ap.c` (`bootUp()`)
- `src/bsp/bsp.c` (`bspDeInit()`)
- (앱) `firmware/stm32h5-fw/src/bsp/ldscript/STM32H563xx_FLASH.ld`
- (앱) `firmware/stm32h5-fw/src/bsp/device/system_stm32h5xx.c`

## VTOR / MSP 책임 분담

부트로더는 **VTOR 도 MSP 도 건드리지 않는다.** 벡터테이블 `+4`(Reset_Handler)에서
함수 포인터를 읽어 직접 호출한다.

```c
void (**jump_func)(void) = (void (**)(void))(FLASH_ADDR_FIRM + FLASH_SIZE_TAG + 4);
// 범위 검사 -> resetSetBootMode(0) -> bspDeInit() -> (*jump_func)();
```

- **MSP** : 앱 `Reset_Handler` 의 `ldr sp, =_estack` 이 설정한다.
- **VTOR** : 앱 `SystemInit()` 이 설정한다.

앱 `system_stm32h5xx.c` 를 다음과 같이 바꿨다.

```c
extern uint32_t _fw_flash_begin;
...
  SCB->VTOR = (uint32_t)&_fw_flash_begin;     // 기존: FLASH_BASE | VECT_TAB_OFFSET
```

convex / apm32e103-kit 에서 검증된 패턴이며, 부트로더가 앱의 메모리 배치를 몰라도
되게 만든다.

## 앱 링커스크립트 변경

```
VECTOR (rx) : ORIGIN = 0x08020400, LENGTH = 1K
VER    (rx) : ORIGIN = 0x08020800, LENGTH = 1K
FLASH  (rx) : ORIGIN = 0x08020C00, LENGTH = 448K-3K
```

**TAG(`0x08020000`, 1KB)는 링커 영역에서 제외한다.** TAG 는 부트로더가 기록하므로
앱 이미지에 포함되면 안 된다. 그래야 `objcopy -O binary` 결과가 `0x08020400` 부터
시작하고, UF2 `targetAddr = 0` ↔ `슬롯 베이스 + FLASH_SIZE_TAG` 매핑이 성립한다.

## bspDeInit()

```c
usbDeInit();  HAL_Delay(50);
__disable_irq();
for (i) { NVIC->ICER[i] = 0xFFFFFFFF;  NVIC->ICPR[i] = 0xFFFFFFFF; }   // pending 도 클리어
SysTick->CTRL = LOAD = VAL = 0;
HAL_MPU_Disable();
__enable_irq();
```

- `ICPR` 을 빠뜨리면 앱이 인터럽트를 켜는 순간 부트로더 시절의 보류 인터럽트가
  앱 벡터로 튄다. 참고 구현(convex)에 빠져 있어 추가했다.
- **RCC De-init 은 하지 않는다.** 앱 `SystemInit()` 이 RCC 를 리셋 상태로 되돌린다.
  여기서 클럭을 내리면 앱 `SystemInit()` 실행 자체가 위태로워진다.

## 검증 로직

`bootVerifySlot(addr)` 하나로 FIRM / SLOT0 / SLOT1 을 모두 검증한다.

1. `firm_tag_t.magic_number == TAG_MAGIC_NUMBER`
2. `fw_size` 가 `0 < fw_size <= FLASH_SIZE_FIRM - FLASH_SIZE_TAG`
3. `fw_addr == FLASH_SIZE_TAG`
4. 본문 CRC16 == `fw_crc`

**TAG 매직을 먼저 확인하고 그 다음에 본문을 읽는 순서를 반드시 지킨다.** TAG 는 항상
마지막에 커밋되므로 "TAG 유효 = 본문 기록 완료" 가 보장되고, 중단된 기록 영역을 읽어
ECC 오류(NMI)를 내는 일을 피할 수 있다.

> 참고 구현(apm32e103-kit, convex)의 상한은 `fw_size >= FLASH_SIZE_FIRM` 인데,
> TAG 1KB 를 빼지 않아 1KB 오버런이 가능하다. 여기서는 `FLASH_SIZE_FIRM - FLASH_SIZE_TAG`
> 로 고쳤다.

### CRC

`utilUpdateCrc()` 는 **CRC-16/BUYPASS**(poly `0x8005`, MSB-first, init `0x0000`)다.
호스트 도구(`test/target/lib/firmtag.py`)에 동일하게 구현하고 테이블 256개가
정확히 일치함을 확인했다.

## bootUp() 판정 (이번 단계 범위)

```c
boot_mode = resetGetBootMode();
if (boot_mode & (1<<MODE_BIT_BOOT))            run_fw = false;   // 앱이 resetToBoot()
if (resetGetCount() >= HW_RESET_DBLCLK_CNT)    run_fw = false;   // 리셋 더블클릭
if (run_fw)  bootJumpFirm();                                     // 성공 시 돌아오지 않음
```

폴트 복구 / 롤백 / `MODE_BIT_UPDATE` 분기는 8~9단계에서 추가한다.

## 호스트 도구

- `test/target/lib/firmtag.py` — CRC16, `firm_tag_t`, `boot_slot_t` 생성
- `test/target/mkimage.py` — 앱 `.bin` 에 TAG 를 붙여 FIRM/SLOT 에 그대로 구울 이미지 생성

```bash
python3 mkimage.py ../../../stm32h5-fw/build/stm32h5-w6300-fw.bin /tmp/firm_v1.bin 1
```

## 검증

```bash
PROG=/opt/ST/STM32CubeCLT_1.21.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI
$PROG -c port=SWD mode=UR -w build/stm32h5-w6300-boot.bin 0x08000000
$PROG -c port=SWD mode=UR -w /tmp/firm_v1.bin 0x08020000 -v
```

## 실측 결과

앱 빌드: `FLASH 139,468 B / 445 KB (30.6%)` — 448KB 슬롯에 여유 있게 들어간다.

이미지 헤더:

```
초기 MSP      = 0x200A0000
Reset_Handler = 0x08024305      (thumb bit 포함, 0x08020400~0x0808FFFF 범위)
fw_size = 141520 (138.2 KB)   fw_crc = 0xD14D   seq = 1
```

### 점프

```
[  ] bootJumpFirm()
     addr : 0x8024305

[ Firmware Begin... ]
Booting..Name  : STM32H5-W6300-FW
Booting..Addr  : 0x8020400          <- 앱이 0x08020400 에서 실행
[OK] usbBegin()  USB_CDC
[OK] wiznetInit()  ID : W6300
cli#
```

앱의 USB CDC 와 W6300 이더넷(DHCP/SNTP)까지 전부 정상 동작했다. 즉 VTOR 이 올바르게
설정되어 인터럽트가 앱 벡터로 들어가고 있다.

### 왕복

앱에서 `reset boot` → 부트로더가 `MODE_BIT_BOOT` 를 감지하고 머무른다.

```
cli# boot info
BOOT
  BOOT   STM32H5-W6300-BOOT  V260822R1
FIRM   0x08020000 valid=1 size=141520 crc=0xD14D (err 0x0)
         STM32H5-W6300-FW  V251024R1
SLOT0  0x08100000 valid=0 seq=0 size=0 crc=0x0000
SLOT1  0x08170000 valid=0 seq=0 size=0 crc=0x0000

write slot    : 0
pending slot  : -1
rollback slot : -1
next seq      : 1
```

부트로더가 계산한 `fw_crc = 0xD14D` 가 호스트에서 계산한 값과 정확히 일치한다.
TAG 생성 → 플래시 → 검증 파이프라인 전체가 맞물렸다.

슬롯이 하나도 없는 상태에서 `rollback slot = -1` 이 나오는 것도 의도대로다.
SWD 로 FIRM 만 직접 구운 상태에서는 롤백하지 않는다.
