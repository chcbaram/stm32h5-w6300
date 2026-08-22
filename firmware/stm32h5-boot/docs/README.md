# stm32h5-boot 문서

STM32H563 용 UF2 / CDC / WebHID 부트로더. 구현 단계별로 번호를 붙여 정리한다.

각 문서는 **목적 → 대상 파일 → 설계 결정과 근거 → 함정/주의사항 → 검증 방법 → 실측 결과** 순서를 따른다.

| 문서 | 내용 |
|---|---|
| [STATUS.md](STATUS.md) | **현재 진행 상황 · 개발 환경 · 실기에서 잡은 함정** |
| [00-memory-map.md](00-memory-map.md) | 플래시/슬롯 배치, 태그 포맷 |
| [01-project-skeleton.md](01-project-skeleton.md) | 디렉토리, CMake, 링커스크립트 |
| [02-hw-layer.md](02-hw-layer.md) | hw_def.h, 이식한 드라이버 |
| [03-flash-driver.md](03-flash-driver.md) | STM32H5 플래시 함정 전수 |
| [04-reset-doubleclick.md](04-reset-doubleclick.md) | 리셋 카운트, POR/PIN 구분 |
| [05-boot-jump.md](05-boot-jump.md) | 검증/점프, VTOR·MSP 책임 분담 |
| [06-usb-composite.md](06-usb-composite.md) | TinyUSB MSC+CDC+HID |
| [07-uf2.md](07-uf2.md) | UF2 파싱, FAT16 가상 디스크 |
| [08-slot-pingpong.md](08-slot-pingpong.md) | 슬롯 선택 규칙, 롤백 |
| [09-fault-recovery.md](09-fault-recovery.md) | 폴트 카운터, 부트 이벤트 로그 |
| [09b-power-loss.md](09b-power-loss.md) | 전원 손실 강건성 |
| [12-app-changes.md](12-app-changes.md) | 앱 측 변경 목록 |
| [13-test.md](13-test.md) | 호스트/타깃 테스트 |
| [14-roadmap.md](14-roadmap.md) | **남은 설계와 결정 근거** |

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


## 웹 업데이터

WebHID 페이지는 **리포 루트**에 있다. 부트로더 전용이 아니라 보드 전체를 다루는
도구(펌웨어 갱신 + 보드 정보 + 향후 테스트)이고, 앱에 HID 가 붙으면 앱과도 통신한다.

```
/index.html          페이지 (GitHub Pages 루트 배포)
/web/proto.js        cmd 패킷 프로토콜 + HID 전송 (부트로더/앱 공용)
/web/boot.js         부트로더 커맨드 셋
/web/panels/*.js     화면 패널. 기능을 추가할 때 여기에 파일을 하나 더 넣는다.
```

ES 모듈이라 `file://` 로는 열리지 않는다. 로컬 확인은 정적 서버로 한다.

```bash
python3 -m http.server 8899      # http://127.0.0.1:8899
```
