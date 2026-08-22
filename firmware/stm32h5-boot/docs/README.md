# stm32h5-boot 문서

STM32H563 용 UF2 / CDC / WebHID 부트로더. 구현 단계별로 번호를 붙여 정리한다.

각 문서는 **목적 → 대상 파일 → 설계 결정과 근거 → 함정/주의사항 → 검증 방법 → 실측 결과** 순서를 따른다.

| 문서 | 내용 | 상태 |
|---|---|---|
| [00-memory-map.md](00-memory-map.md) | 플래시/슬롯 배치, 태그 포맷 | 확정 |
| [01-project-skeleton.md](01-project-skeleton.md) | 디렉토리, CMake, 링커스크립트 | 완료 |
| [02-hw-layer.md](02-hw-layer.md) | hw_def.h, 이식한 드라이버 | 완료 |
| [03-flash-driver.md](03-flash-driver.md) | STM32H5 플래시 함정 전수 | 완료 |
| 04-reset-doubleclick.md | 리셋 카운트, POR/PIN 구분 | 예정 |
| 05-boot-jump.md | 검증/점프, VTOR·MSP 책임 분담 | 예정 |
| 06-usb-composite.md | TinyUSB MSC+CDC+HID | 예정 |
| 07-uf2.md | UF2 파싱, FAT16 가상 디스크 | 예정 |
| 08-slot-pingpong.md | 슬롯 선택 규칙, 롤백 | 예정 |
| 09-fault-recovery.md | 폴트 카운터, 부트 이벤트 로그 | 예정 |
| 09b-power-loss.md | 전원 손실 강건성 | 예정 |
| 10-cmd-protocol.md | 패킷 포맷, CDC/HID 채널 | 예정 |
| 11-webhid-updater.md | WebHID 업데이터 | 예정 |
| 12-app-changes.md | 앱 측 변경 목록 | 예정 |
| 13-test.md | 호스트/타깃 테스트 | 예정 |

호스트 유닛 테스트는 하드웨어 없이 돈다.

```bash
cd firmware/stm32h5-boot/test/host && ./run.sh
```

## 빌드

```bash
cd firmware/stm32h5-boot
cmake -S . -B build
cmake --build build -j8
```

## 플래시 / 디버그

VSCode 태스크(`Cmd+Shift+P` -> Tasks: Run Task)와 디버그 구성이 준비되어 있다.
태스크는 `Build - ` / `Flash - ` / `Device - ` / `Test - ` / `Setup - ` / `App - `
접두어로 묶여 있다.

### pyocd (권장)

STM32H5 는 pyocd 내장 타겟이 아니므로 **CMSIS-Pack 을 한 번 설치**해야 한다.

```bash
pyocd pack install stm32h563ritx          # 태스크: Setup - pyocd install pack
pyocd flash -t stm32h563ritx --frequency 4000000 \
      --base-address 0x08000000 build/stm32h5-w6300-boot.bin
```

정확한 품번은 **STM32H563RITx** 다(`stm32h5-cube/stm32h5-w6300.ioc` 의 `Mcu.Name`).

### STM32CubeCLT

macOS 12 에서는 CLT 1.22 가 실행되지 않는다(Qt 가 macOS 13 이상 요구). **1.21.0 을 쓴다.**

```bash
PROG=/opt/ST/STM32CubeCLT_1.21.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI
$PROG -c port=SWD mode=UR -w build/stm32h5-w6300-boot.bin 0x08000000 -v
```

### 디버그

`launch.json` 에 세 가지가 있다.

| 구성 | 용도 |
|---|---|
| Debug (pyocd) | 빌드 -> 플래시 -> `main` 정지 |
| Attach (pyocd) | 돌고 있는 펌웨어에 붙는다(플래시 안 함) |
| Debug (ST-LINK GDB server) | STM32CubeCLT 의 gdbserver 사용 |

### OpenOCD 는 쓰지 않는다

확인 결과 이 조합으로는 동작하지 않는다.

- OpenOCD 0.12.0(homebrew)에 **STM32H5 플래시 드라이버(`stm32h5x`)가 없다.**
  바이너리에서 `stm32f1x/f2x/h7x/l4x/lx` 만 확인된다. 즉 플래시 자체가 불가능하다.
- 디버그 attach 도 실패한다. 보유한 ST-LINK V2(V2J45M30)는 `dap` 전송을 지원하지
  않아 `hla_swd` 로 붙어야 하는데, Cortex-M33 타겟 examination 이 실패한다.

pyocd 는 CMSIS-Pack 으로 플래시·디버그가 모두 정상 동작하므로 pyocd 와 ST-LINK
GDB server 만 쓴다.

## CLI

ST-LINK 의 VCP(SWD 커넥터의 UART)로 접속한다. 개행은 **CR(0x0D)** 이다.

```
포트 : /dev/cu.usbmodem1412102   115200 8N1
```
