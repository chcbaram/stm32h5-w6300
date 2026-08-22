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
| 9b | 호스트 유닛 테스트 | ✅ 50 passed |
| 10 | cmd 프로토콜 (CDC) | ✅ 134 KB/s |
| 11 | HID 채널 + WebHID 페이지 | ✅ 38 KB/s |
| 12 | 앱 연동 | ✅ |
| 13 | 타깃 통합 테스트 | ✅ 22 passed |
| + | HID CLI + 웹 CLI 패널 | ✅ |
| 15 | **앱에 HID(ST composite) + cmd 채널** | ✅ 실기 검증 |
| 16 | **보드 시각(RTC epoch)** — 부트 로그 시각 · 웹 동기화 | ✅ 실기 검증 |
| 17 | **네트워크 상태 조회** — 웹에서 IP/MAC, 보드 웹페이지로 이동 | ✅ 실기 검증 |
| 18 | **LAN 스캔 + 이더넷 OTA(TCP)** | ✅ 가짜 보드로 양방향 검증 |
| 19 | **보드 자체 웹서버** — HTTP + POST /cmd | ✅ 브라우저 확인 완료 |
| 20 | **MAC 을 UID 에서 만들기** | ✅ 실기 검증 (`02:00:46:00:D2:FC`) |

**빌드**: 부트로더 85,848 B / 126 KB (66.5%) · 앱 192,336 B / 445 KB (42.2%)
**빌드 경고**: 세 빌드(앱·부트로더·호스트 테스트) 모두 0

앱도 이제 부트로더와 **같은 VID/PID·같은 커맨드 셋**으로 열거된다. 웹페이지는
부트로더/앱 어느 쪽에 붙어도 동작하고, 앱 상태에서 슬롯 스테이징과 부트로더
재진입까지 된다. 자세한 것은 `15-app-hid.md`.

## 다음에 할 일

우선순위 순. 설계 근거는 전부 `14-roadmap.md` 에 있다.

1. **타깃 테스트 보강** — `test_06_net.py` 로 TCP OTA·HTTP 서버·LAN 스캔·RTC 를
   덮는다. 지금 회귀를 잡아주는 것이 호스트 유닛 50개뿐이고 그건 부트로더 슬롯
   로직만 본다. 15~20 단계에서 들어온 것이 전부 테스트 밖이다.
2. **NVS 가상 EEPROM** (`14-roadmap.md` D). 보드 이름·고정 IP 를 여기 둔다
3. **부트 타임아웃** — 조건(FIRM 유효 + 호스트 미연결)까지 정리했으나 보류 중

### 보류한 것 — mDNS 응답기

`stm32h5-d2fc.local` 로 IP 없이 접속하는 것. **소켓이 없어서 미룬다.**
0~7 을 telnet/DHCP/SNTP/cmd TCP/discovery/HTTP×3 이 다 쓴다.

비우는 방법은 있었다.

| | 내용 | 대가 |
|---|---|---|
| A | cmd TCP(3)를 HTTP POST 로 대체 | 네트워크 cmd 가 HTTP 전용이 된다. PC 가 아닌 장치가 붙을 때 불리 |
| B | HTTP 를 3 -> 2 소켓 | 브라우저가 파일 2개 + favicon 까지 요청하면 아슬아슬 |
| C | **안 한다** | 지금 선택 |

C 로 간 이유는, mDNS 가 주는 것이 "IP 대신 이름을 친다" 하나인데 **보드를 찾는
문제는 이미 스캔이 더 잘 풀기** 때문이다. 비컨에 이름·버전·모드가 다 실려 오고,
`discover.py --open` 으로 바로 열리고, 웹에 IP 입력칸도 있다.

나중에 이름으로 접속하는 것이 정말 필요해지면 A 가 가장 깨끗하다.

### 알려진 정리거리 — 소켓 표가 현실과 다르다

`hw_def.h` 의 `HW_WIZNET_SOCKET_*` 가 실제와 어긋난다.

- `HW_WIZNET_SOCKET_CMD 0` 은 이름과 달리 **텔넷 CLI** 다(`cli_net.c` 의 `cli_sn`)
- **4~7 이 표에 없다.** `net_disc.c` 의 `DISC_SN 4`, `net_http.c` 의
  `HTTP_SN_FIRST 5` 가 각자 하드코딩되어 있다

소켓은 8개뿐인 자원이라 한 곳에서 다 보여야 충돌을 막는다. 실제로 mDNS 자리를
찾다가 헷갈렸다. 기능 변경은 아니지만 다음에 손볼 때 같이 정리한다.

로드맵 A·B·C·E 는 끝났다. 각각 `15`, `18`, `19`, `18` 문서를 본다.

## 개발 환경 (실측으로 확정된 것들)

| 항목 | 값 |
|---|---|
| MCU 품번 | **STM32H563RITx** (`stm32h5-cube/stm32h5-w6300.ioc` 의 `Mcu.Name`) |
| STM32CubeCLT | **1.21.0** 을 쓴다. macOS 12 에서 1.22 는 실행되지 않는다(Qt 가 macOS 13 이상 요구) |
| pyocd | `pyocd pack install stm32h563ritx` 를 한 번 해야 한다. 그 뒤 flash/debug 정상 |
| OpenOCD | **쓸 수 없다.** 0.12.0 에 STM32H5 플래시 드라이버가 없고, ST-LINK V2 로는 M33 examine 도 실패 |
| ST-LINK VCP | `/dev/cu.usbmodem1412102` (부트/앱 CLI) |
| 보드 USB CDC | 부트 모드 `/dev/cu.usbmodem1412301` · 앱 `/dev/cu.usbmodem387B387A31331` (앱은 UID 기반 시리얼이라 이름이 다르다) |
| HID | VID `0xCAFE` PID `0xB003` usage_page `0xFF00` — **부트로더와 앱이 동일** |
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
#   또는 웹페이지 '부트로더로 재진입' 버튼 / HID BOOT_CMD_FW_JUMP(0x0009)

# 앱이 실행 중인 채로 슬롯에 스테이징 (적용은 재부팅 후 부트로더가)
python3 tools/download/download.py <bin> --port <앱 CDC>

# CDC 스트림의 주인은 호스트가 연 보율이 정한다
#   115200 -> CLI (미니컴/터미널)
#   그 외   -> cmd (download.py 는 921600)
#   지금 누가 쥐고 있는지는 CLI 의 `usb info` 에서 CDC Owner 로 보인다

# 문서 그림을 고치면 다시 만든다 (SVG 는 손으로 편집하지 않는다)
cd firmware/stm32h5-boot/docs/images
python3 gen_memory_map.py      # 메모리 맵
python3 gen_diagrams.py        # 슬롯 핑퐁 / 롤백 / 폴트 복구 / OTA 경로
#   확인 : rsvg-convert -b '#ffffff' -o /tmp/a.png <파일>.svg   (밝은 배경)
#          rsvg-convert -b '#0d1117' -o /tmp/b.png <파일>.svg   (어두운 배경)
#   렌더한 PNG 를 실제로 눈으로 볼 것. 글자가 잘리거나 겹치는 것은 그래야 보인다.

# 웹페이지를 고치면 반드시 다시 만들어야 보드에 반영된다
python3 firmware/stm32h5-fw/tools/web/gen_web.py . firmware/stm32h5-fw/src/ap/modules/net/web_page.c

# 웹 JS 검사 (macOS 의 JXA. 브라우저 없이 문법과 번들 실행까지 본다)
osascript -l JavaScript <검사 스크립트> web/*.js

# LAN 의 보드 찾기 / 이더넷으로 업데이트
python3 firmware/stm32h5-fw/tools/net/discover.py                 # PC 가 스캔
python3 firmware/stm32h5-fw/tools/net/discover.py --serve         # 가짜 보드(시험용)
python3 firmware/stm32h5-fw/tools/download/download.py <bin> --tcp <IP>
#   보드 CLI 에서는 `scan`

# 보드 시각 (코인셀이 없어 전원을 뽑으면 지워진다)
#   CLI      : rtc get info / rtc set date [y] [m] [d] / rtc set time [h] [m] [s]
#   웹       : 보드 정보 탭 -> PC 시간과 맞추기
#   커맨드   : BOOT_CMD_RTC(0x0012), [0]=op(0 GET/1 SET), SET 이면 [1..4]=epoch

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
10. **CDC 스트림을 CLI 와 cmd 패킷이 나눠 먹으면 안 된다.** 둘 다 `cdcRead()` 를
    부르면 서로 바이트를 훔쳐 다운로드가 첫 `FW_WRITE` 에서 끊긴다.
    **호스트가 연 보율로 주인을 가른다** — 115200 이면 CLI, 그 외면 cmd.
    호스트 툴은 921600 으로 연다(`download.py` 의 `CMD_BAUD`).
11. **USB_DRD_FS 의 BTABLE 은 EP 당 8바이트, 8개면 `0x000~0x03F`.** 기존 CDC 전용
    PMA 배치는 EP0 버퍼를 `0x014` 에 두어 BTABLE 을 침범하고 있었다. EP3(HID)를
    추가하는 순간 깨진다.
12. **`USBD_RegisterClassComposite()` 는 끝에서 `classId++` 를 한다.** 바로 뒤에
    `USBD_xxx_RegisterInterface()` 를 부르면 다른 클래스 슬롯에 fops 가 저장된다.
    ST 예제 코드가 이 순서다. `classId` 를 되돌려 놓고 불러야 한다.
13. **`USBD_COMPOSITE_USE_IAD` 는 기본값이 없다.** 정의하지 않으면 IAD 가 조용히
    빠지고, `0xEF/0x02/0x01` 장치에서 윈도우가 CDC VCP 를 만들지 못한다.
14. **`USBD_static_malloc()` 이 고정 버퍼를 돌려주면 composite 에서 클래스끼리
    메모리를 공유한다.** bump 할당기로 바꾸고 `usbBegin()` 마다 되감는다.
15. **`cliMgrEnable(false)` 는 포트 전환까지 막아야 한다.** `cliMain()` 만 막으면
    cmd 채널의 CLI 가 가상 포트를 잡고 있는 동안 `cli_mgr` 이 포트를 되돌려 버린다.
    그러면 `cliKeepLoop()` 이 엉뚱한 포트를 보고 반복 명령이 영원히 돈다.
16. **cmd 채널의 CLI 에는 키를 눌러줄 사람이 없다.** `cliKeepLoop()` 을 쓰는 반복
    명령(`usb info` 등)은 `drvCliUpdate()` 가 200ms 뒤에 개행을 넣어 끊어준다.
17. **CLI 에코를 응답으로 오인하지 말 것.** 115200 으로 열면 CLI 가 입력을 그대로
    돌려주는데 그게 `STX0 STX1` 로 시작한다. 호스트는 `type == RESP` 이고 `cmd` 가
    일치할 때만 응답으로 받는다.
18. **버퍼가 넘칠 때 조용히 버리지 말 것.** 웹 CLI 출력이 1KB 에서 잘리고 있었는데
    호스트는 그게 전부인 줄 알았다. 지금은 조각내어 보내고, 그래도 넘치면 꼬리에
    `... (출력이 잘렸다)` 를 남긴다.
19. **웹 패널의 refresh() 는 비동기다.** 기다리는 동안 탭이 바뀌면 DOM 이 갈린다.
    `await` 뒤마다 `ctx.isActive(id)` 로 확인한다. 오류 응답을 검사 없이 파싱하면
    "Offset is outside the bounds of the DataView" 가 난다.
20. **보드 시각은 지역시 접근자로 읽으면 안 된다.** epoch 을 "달력 필드를 UTC 로
    간주해" 만들기 때문에, `getHours()` 로 찍으면 타임존만큼(KST 9시간) 밀린다.
    표시는 전부 `epochToText()` 를 거친다.
21. **같은 펌웨어를 다시 올리면 적용할 것이 없다.** 슬롯 내용이 FIRM 과 같으면
    `bootGetPendingSlot()` 이 -1 이라 부트로더가 아무 일도 하지 않고, 부트 이벤트
    로그에도 남지 않는다. 그냥 리셋하면 호스트는 "적용됐다" 고 잘못 안다.
    앱이 `ERR_BOOT_NO_PENDING(0x0016)` 으로 알려주고 리셋하지 않는다.
22. **웹의 부트 이벤트 표는 재연결 때 비운다.** 이전 연결의 목록이 남아 있으면
    업데이트 뒤 재연결했을 때 "기록이 안 남았다" 고 오해한다. 보드 플래시의
    로그는 건드리지 않는다(지우려면 CLI 의 `boot log clear`).
23. **`faultGetPc()` 는 `.noinit` 값을 영원히 돌려주고 있었다.** 폴트가 한참 전에
    한 번 났으면 그 PC 가 폴트와 무관한 롤백/검증 실패 기록에도 찍혔다. 전원을 막
    넣은 뒤면 SRAM 쓰레기값(0x20000000 같은)이 나왔다. `faultInit()` 이 매직을 보고
    한 번만 걷어 담고, 그 뒤로는 0 을 준다.
24. **IP 가 보인다고 받은 IP 가 아니다.** `wiznet.c` 의 `net_info` 에 기본값이
    박혀 있어 케이블이 빠져 있어도 그 값이 읽힌다. `is_ip_get` 으로 판단한다.
25. **구조체에 자리만 있고 채우지 않는 필드는 없느니만 못하다.** `wiznetGetInfo()`
    가 `dhcp` 를 복사하지 않아 호출자가 스택 쓰레기값을 봤다.
26. **W6300 은 소켓이 8개인데 5개를 이미 쓴다.** HTTP 에 셋뿐이라 브라우저의 동시
    요청을 다 못 받는다. 파일 수를 줄이는 것(모듈 번들)이 근본 해결이다.
27. **여러 줄 `import` 는 줄 단위 정규식이 놓친다.** 문법 검사는 통과하므로
    번들을 **실행**해봐야 잡힌다. macOS 의 JXA(`osascript -l JavaScript`)로 한다.
28. **브라우저가 옛 모듈을 캐시하면 페이지가 통째로 죽는다.** 새 이름을 import
    하는데 옛 파일에 없으면 링크 실패 -> 스크립트 전체가 안 돈다. 하드 리로드.
29. **UID(0x08FFF800)는 워드로만 읽힌다.** 바이트로 훑으면 하드폴트다.
    `mpuInit()` 이 non-cacheable 로 잡아 접근이 버스로 바로 나가기 때문이다.
    `utilCalcCRC()` 에 UID 주소를 그대로 넘겼다가 걸렸다. 워드로 복사한 뒤 계산한다.
30. **MAC 을 바꾸면 DHCP 가 새 IP 를 준다.** 이더넷으로 올린 뒤 그 IP 로는 못 찾는다.
    `discover.py` 로 다시 찾는다.
31. **macOS 의 "디스크를 제대로 꺼내지 않았습니다" 는 못 없앤다.** `TEST UNIT READY`
    거부도, 읽기/쓰기 실패도 소용없었다(실측). 경고 없는 갱신은 CDC/HID 경로뿐이다.
    시험 명령은 `uf2 eject / insert / unplug / plug` 로 남겨뒀다.

## 공용 파일 (한쪽 고치면 반대쪽에 복사)

```
src/common/hw/include/{reset.h, fault.h, cmd.h}
src/hw/driver/{reset.c, fault.c, flash.c, uart.c, cmd.c}
src/ap/modules/boot/{boot.c, boot.h, boot_log.c, boot_log.h}
src/ap/modules/common/cli/cli_mgr.c
src/ap/modules/cmd/{cmd_task.c, cmd_task.h}
src/ap/modules/cmd/driver/{drv_usb.c, drv_cli.c}
src/ap/modules/cmd/process/cmd_boot.c
```

내용이 갈리는 두 파일:

- **`drv_hid.c`** — 부트로더는 TinyUSB(`tud_hid_report()`), 앱은 ST 스택(`hidWrite()`).
  리포트 규약(선두 바이트 = 유효 길이)은 같다.
- **`uart.c`** — 채널 표(`uart_hw_tbl`)와 `HW_UART_MAX_CH` 만 다르다. 앱은 텔넷용
  NET 채널이 하나 더 있어 4개다. 로직은 같으므로 고칠 때 함께 본다.

그 외 동작 차이는 `HW_RESET_BOOT` / `FLASH_PROTECT_*` / `HW_DEV_MODE` 매크로로만
갈린다. `cmd_boot.c` 의 `FW_UPDATE`/`FW_JUMP` 도 `HW_DEV_MODE` 로 분기한다.
