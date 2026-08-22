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

## 빌드

```bash
cd firmware/stm32h5-boot
cmake -S . -B build
cmake --build build -j8
```

## 플래시

macOS 12 에서는 STM32CubeCLT 1.22 가 동작하지 않는다(Qt 가 macOS 13 이상 요구).
**1.21.0 을 쓴다.**

```bash
PROG=/opt/ST/STM32CubeCLT_1.21.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI
$PROG -c port=SWD mode=UR -w build/stm32h5-w6300-boot.bin 0x08000000 -v
```

## CLI

ST-LINK 의 VCP(SWD 커넥터의 UART)로 접속한다. 개행은 **CR(0x0D)** 이다.

```
포트 : /dev/cu.usbmodem1412102   115200 8N1
```
