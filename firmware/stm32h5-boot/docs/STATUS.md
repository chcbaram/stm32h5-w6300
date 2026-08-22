# 작업 현황

마지막 갱신: 2026-08-22

## 어디까지 됐나

부트로더가 실기에서 **전 경로 동작**한다. UF2 드래그&드롭 / CDC 다운로드 /
HID 다운로드 / HID CLI / 폴트 자동 복구 / 슬롯 핑퐁 롤백.

| 단계 | 내용 | 상태 |
|---|---|---|
| 1–2 | 골격 · hw 계층 · CLI | ✅ |
| 3 | flash.c | ✅ |
| 4 | 리셋 더블클릭 | ✅ (물리 버튼 실측) |
| 5 | 검증 · 점프 | ✅ |
| 6 | TinyUSB MSC+CDC+HID | ✅ |
| 7 | UF2 + FAT16 디스크 | ✅ |
| 8 | 슬롯 핑퐁 | ✅ |
| 9 | 폴트 복구 + 부트 로그 | ✅ |
| 9b | 호스트 유닛 테스트 | ✅ 44 passed |
| 10 | cmd 프로토콜 (CDC) | ✅ 134 KB/s |
| 11 | HID 채널 + WebHID 페이지 | ✅ 38 KB/s |
| 12 | 앱 연동 | ✅ |
| 13 | 타깃 통합 테스트 | ✅ 11 passed |
| + | HID CLI + 웹 CLI 패널 | ✅ |

**빌드**: 부트로더 83,368 B / 126 KB (64.6%) · 앱 148,976 B / 445 KB (32.7%)

## 다음에 할 일

우선순위 순. 설계 근거는 전부 `14-roadmap.md` 에 있다.

1. **앱에 HID 추가** — ST composite. convex(`convex-qmk/src/hw/driver/usb/usb_cmp`,
   `usb_hid`)에서 `usbd_cmp.c`/`usbd_hid.c` 를 가져온다. 엔드포인트 테이블만 H5 PMA 에
   맞게 다시 잡고, usage page 는 0xFF00 으로.
2. **앱에 `cmd_hid.c` + `cli_cmd.c`** — 그러면 웹에서 앱 CLI 도 된다.
3. **네트워크 패널** — 앱이 자기 IP 를 알려주고 LAN 을 스캔해 목록을 돌려준다.
4. **보드 자체 웹서버** — W6300 위 HTTP/WebSocket. 페이지는 뱅크1 예약 448KB 에.
5. NVS, 이더넷 OTA (`14-roadmap.md` D·E)

## 개발 환경 (실측으로 확정된 것들)

| 항목 | 값 |
|---|---|
| MCU 품번 | **STM32H563RITx** (`stm32h5-cube/stm32h5-w6300.ioc` 의 `Mcu.Name`) |
| STM32CubeCLT | **1.21.0** 을 쓴다. macOS 12 에서 1.22 는 실행되지 않는다(Qt 가 macOS 13 이상 요구) |
| pyocd | `pyocd pack install stm32h563ritx` 를 한 번 해야 한다. 그 뒤 flash/debug 정상 |
| OpenOCD | **쓸 수 없다.** 0.12.0 에 STM32H5 플래시 드라이버가 없고, ST-LINK V2 로는 M33 examine 도 실패 |
| ST-LINK VCP | `/dev/cu.usbmodem1412102` (부트/앱 CLI) |
| 보드 USB CDC | 부트 모드에서 `/dev/cu.usbmodem14123xx` 로 추가 열거 |
| HID | VID `0xCAFE` PID `0xB003` usage_page `0xFF00` |
| MSC 볼륨 | `/Volumes/H5BOOT` |
| CLI 개행 | **CR(0x0D)**. `\n` 을 보내면 에코만 되고 실행되지 않는다 |

```bash
PROG=/opt/ST/STM32CubeCLT_1.21.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI
```

## 자주 쓰는 명령

```bash
# 빌드
cd firmware/stm32h5-boot && cmake -S . -B build && cmake --build build -j8
cd firmware/stm32h5-fw   && cmake -S . -B build && cmake --build build -j8   # .uf2 도 나온다

# 플래시
$PROG -c port=SWD mode=UR -w build/stm32h5-w6300-boot.bin 0x08000000 -v
pyocd flash -t stm32h563ritx --frequency 4000000 --base-address 0x08000000 build/stm32h5-w6300-boot.bin

# 테스트
cd firmware/stm32h5-boot/test/host   && ./run.sh              # 하드웨어 불필요, 44 passed
cd firmware/stm32h5-boot/test/target && python3 -m pytest     # 실기 필요

# 부트 모드 진입 (물리 버튼 없이)
#   앱 CLI 에서 reset boot

# 웹 페이지 (ES 모듈이라 file:// 로는 안 열린다)
cd <repo root> && python3 -m http.server 8899
```

## 실기에서 잡은 함정들 (다시 밟지 말 것)

1. **`HAL_FLASH_Program` 은 쿼드워드 재기록을 막아주지 않는다.** `HAL_OK` 를 반환하고
   ECC 만 깨진다. 그 워드를 읽는 순간 NMI. → `flashWrite` 에 blank check 필수.
2. **`NMI_Handler` 의 `while(1)`** → 위 상황에서 부트로더가 영구 정지. 오류 위치를
   백업 레지스터에 남기고 리셋하도록 교체했다.
3. **STM32H5 는 소프트 리셋에서도 `PINRSTF` 가 선다**(내부 리셋이 NRST 로 전파).
   SOFT/WDG 를 PIN 보다 먼저 걸러야 `resetToBoot()` 마다 오진입하지 않는다.
4. **ST-LINK 로는 순수 NRST 를 만들 수 없다.** `mode=UR` 이든 `-rst` 든 소프트 리셋을
   동반한다. 더블클릭은 물리 버튼으로만 검증된다.
5. **`tud_msc_write10_complete_cb` 가 전송 후에도 계속 불린다.** 가드가 없으면 이미
   기록한 태그에 재기록을 시도한다.
6. **`.non_cache` orphan 섹션** — `uart_tbl` 이 startup 의 복사·클리어 어디에도 안 들어가
   부팅 시 쓰레기값이었다. 두 프로젝트 모두 제거했다.
7. **`uf2conv.py --base 0x0` 를 반드시 명시**한다. 기본값이 `0x2000` 이다.
8. **`cli.c` 는 '현재 열린 포트' 로 출력한다.** HID CLI 는 명령 실행 동안만 포트를
   가상 채널로 돌렸다가 복원해야 한다.
9. **pyserial `read(n)` 은 n 바이트를 다 기다린다.** 응답이 10바이트뿐인데 512 를
   요청해 매번 타임아웃을 까먹어 2.4 KB/s 였다. 1바이트만 기다린 뒤 `in_waiting` 을
   몰아 읽어 134 KB/s 가 됐다.

## 공용 파일 (한쪽 고치면 반대쪽에 복사)

```
src/common/hw/include/{reset.h, fault.h}
src/hw/driver/{reset.c, fault.c, flash.c, uart.c}
src/ap/modules/boot/{boot.c, boot.h, boot_log.c, boot_log.h}
src/ap/modules/common/cli/cli_mgr.c
```

동작 차이는 `HW_RESET_BOOT` / `FLASH_PROTECT_*` / `HW_DEV_MODE` 매크로로만 갈린다.
