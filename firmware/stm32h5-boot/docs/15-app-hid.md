# 15. 앱에 HID 추가 (ST composite) + cmd 채널

## 목적

부트로더에만 있던 HID / cmd 패킷 채널을 **앱에도** 붙인다. 그래야 웹페이지가
"부트로더일 때만 쓸 수 있는 도구" 에서 "보드를 항상 다룰 수 있는 도구" 가 된다.

결과:

- 앱 상태에서도 웹페이지가 붙는다 (`BOOT_CMD_INFO` 의 `mode` 로 구분)
- 웹 CLI 패널이 앱 CLI 를 그대로 돌린다
- 앱이 실행 중인 채로 펌웨어를 슬롯에 받아둘 수 있다 (적용은 재부팅 후 부트로더가)
- 웹에서 **부트로더로 재진입**할 수 있다. 리셋 버튼을 두 번 누를 필요가 없다

## 대상 파일

**새로 만든 것** — `firmware/stm32h5-fw/src/hw/driver/usb/`

```
usb_cmp/usbd_composite_builder.{c,h}   ST 공식 빌더. convex 것을 가져와 HID 부분만 교체
usb_cmp/usbd_desc_cmp.{c,h}            composite 장치 디스크립터 (CMP_Desc)
usb_hid/usbd_hid.{c,h}                 vendor HID 클래스. IN/OUT 각 64B
hid.{c,h}                              hw 계층 래퍼 (hidWrite / hidIsReady)
```

채널 드라이버 파일은 `drv_` 접두어로 통일했다(`ap/modules/cmd/driver/`).
내부 함수·변수도 같은 접두어를 쓴다.

| 파일 | 역할 | 심볼 |
|---|---|---|
| `drv_usb.c` | CDC 채널 | `drv_usb_driver`, `drvUsb*()` |
| `drv_hid.c` | HID 채널 | `drv_hid_driver`, `drvHid*()` |
| `drv_cli.c` | cmd 위의 가상 CLI 채널 | `drvCli*()` |

**앱에 이식한 것** — 부트로더와 **같은 내용**

```
src/hw/driver/cmd.c
src/ap/modules/cmd/{cmd_task.c, cmd_task.h}
src/ap/modules/cmd/driver/{drv_usb.c, drv_cli.c}
src/ap/modules/cmd/process/cmd_boot.c
```

`drv_hid.c` 만 두 프로젝트가 다르다. 부트로더는 TinyUSB(`tud_hid_report()`),
앱은 ST 스택(`hidWrite()`)을 부른다. **리포트 규약은 동일**하므로 호스트 코드는
어느 쪽에 붙었는지 몰라도 된다.

## 설계 결정과 근거

### 왜 ST composite 인가 (TinyUSB 교체가 아니라)

`14-roadmap.md` A 에 적어둔 그대로다. 앱의 USB 스택을 유지하면 회귀 범위가
디스크립터와 엔드포인트로 좁혀진다. 스택을 갈면 인터럽트·클럭·PMA·CDC 를 전부
다시 검증해야 한다.

확인해보니 앱의 `STM32_USB_Device_Library` 는 **이미 composite 를 지원**하고 있었다
(`USBD_RegisterClassComposite`, `USE_USBD_COMPOSITE` 가드가 core/ctlreq/cdc 전부에
들어 있다). 빠져 있던 것은 빌더 파일 하나뿐이었다.

### 왜 부트로더와 같은 VID/PID 인가

`hw_def.h` 에 이미 적어둔 결정을 따랐다. 호스트(웹/툴)는 필터 한 벌로 두 상태를
모두 잡고, 어느 쪽인지는 `BOOT_CMD_INFO` 의 `HW_DEV_MODE` 로 안다. VID/PID 를
가르면 웹 필터와 툴 설정을 두 벌 관리해야 하고 장치 선택창에도 항목이 둘로 뜬다.

```
VID 0xCAFE  PID 0xB003  usage page 0xFF00
```

리포트 디스크립터는 TinyUSB 의 `TUD_HID_REPORT_DESC_GENERIC_INOUT(64)` 전개 결과와
**바이트 단위로 동일**하게 손으로 적었다(34바이트). 여기가 어긋나면 웹페이지의
`usagePage` 필터가 앱을 걸러내지 못한다.

### 인터페이스 배치

CDC 를 먼저 등록해 ITF0/ITF1 을 차지하게 한다. 윈도우는 IAD 로 묶인 CDC 가
인터페이스 0 에서 시작할 때 가장 얌전하게 붙는다.

```
ITF0  CDC Comm   EP 0x82 (notif, 8B)
ITF1  CDC Data   EP 0x81 IN / 0x01 OUT (64B, bulk)
ITF2  HID        EP 0x83 IN / 0x03 OUT (64B, interrupt, bInterval 1)
```

실측 구성 디스크립터 = **107바이트, bNumInterfaces 3**. `usb desc` CLI 로 언제든
덤프할 수 있게 해두었다.

## 함정 (전부 실제로 밟았거나 밟기 직전이었다)

### 1. PMA 배치가 BTABLE 을 침범하고 있었다

기존 CDC 전용 배치는 EP0 OUT 버퍼를 `0x014` 에 두고 있었다. USB_DRD_FS 의 BTABLE 은
**엔드포인트당 8바이트** 라 8개면 `0x000~0x03F` 를 차지한다. 즉 EP0 데이터가
EP2 이상의 BTABLE 엔트리 위에 얹혀 있었다.

CDC 만 쓸 때는 EP3~7 이 놀고 있어 드러나지 않았지만, **HID(EP3) 를 추가하는 순간**
EP3 의 버퍼 주소가 EP0 데이터에 덮인다. BTABLE 영역 전체를 비우고 다시 잡았다.

```
0x000  BTABLE          64B
0x040  EP0 OUT         64B
0x080  EP0 IN          64B
0x0C0  CDC notif       16B
0x0D0  CDC IN          64B
0x110  CDC OUT         64B
0x150  HID IN          64B
0x190  HID OUT         64B
       ----------------------
       0x1D0 = 464B / 2048B
```

### 2. `USBD_static_malloc()` 이 클래스마다 같은 버퍼를 돌려줬다

기존 구현은 고정 버퍼 하나를 반환했다. composite 는 클래스마다 한 번씩 부르므로
CDC 와 HID 가 **같은 메모리를 공유**해 서로의 상태를 덮어쓴다. bump 할당기로 바꾸고,
`usbBegin()` 진입 시 `USBD_static_reset()` 으로 되감는다.

### 3. `USBD_xxx_RegisterInterface()` 호출 순서 — ST 예제가 틀렸다

```c
pdev->pUserData[pdev->classId] = fops;      // RegisterInterface 내부
```

그런데 `USBD_RegisterClassComposite()` 는 **끝에서 `classId++` 를 한다.** 등록 직후에
`RegisterInterface` 를 부르면 다음 클래스의 슬롯(또는 배열 밖)에 저장되고, 정작 자기
클래스의 `Init()` 은 `pUserData` 가 NULL 인 채로 돈다. ST 예제 코드와 convex 가
모두 이 순서다.

`classId` 를 되돌려 놓고 부른다.

```c
cdc_class_id = USBD_Device.classId;
USBD_RegisterClassComposite(..., CLASS_TYPE_CDC, cdc_ep_tbl);
hid_class_id = USBD_Device.classId;
USBD_RegisterClassComposite(..., CLASS_TYPE_HID, hid_ep_tbl);

USBD_Device.classId = cdc_class_id;  USBD_CDC_RegisterInterface(...);
USBD_Device.classId = hid_class_id;  USBD_HID_RegisterInterface(...);
USBD_Device.classId = 0;
```

같은 이유로 송신 시점에 `pdev->classId` 를 믿으면 안 된다. `pdev->classId` 는
"지금 처리 중인 클래스" 라서 USB 인터럽트가 세팅한 값이다. `usbGetCdcClassId()` /
`usbGetHidClassId()` 로 등록 시점의 값을 기억해 쓴다.

(SOF 콜백 안은 예외다. `USBD_LL_SOF()` 가 클래스별로 `classId` 를 세팅한 뒤
부르므로 거기서는 신뢰할 수 있다.)

### 4. IAD 가 빠져 있었다

빌더의 IAD 코드는 `USBD_COMPOSITE_USE_IAD == 1` 로 가드되어 있는데 **기본값이 없다.**
정의하지 않으면 `#if 미정의 == 1` 이 거짓이 되어 조용히 빠진다.

장치 디스크립터는 `0xEF/0x02/0x01` (Miscellaneous, IAD) 로 선언해 놓고 IAD 를 빼면
윈도우가 CDC 의 두 인터페이스를 별개 장치로 보고 VCP 를 만들지 못한다.
`usbd_conf.h` 에 `USBD_COMPOSITE_USE_IAD 1` 을 명시했다.

### 5. CDC 스트림을 CLI 와 패킷이 나눠 먹고 있었다 ← 가장 중요

`cli_mgr` 은 `cdcIsConnect()` 면 CLI 포트를 `HW_UART_CH_USB` 로 바꾸고,
`uart.c` 의 `_DEF_UART2` 는 `cdcRead()` 를 부른다. 한편 `drv_usb.c` 도 `cdcRead()` 를
부른다. **둘이 같은 스트림에서 서로 바이트를 훔쳐간다.**

부트로더에서는 링크 순서 덕에 우연히 동작하고 있었다. 앱에 붙이자마자 드러났다 —
`FW_WRITE` 첫 패킷에서 응답이 끊긴다.

```
TimeoutError: cmd 0x0004 응답 없음 (받은 1B)
```

`cmdReceivePacket()` 은 STX0 이 아닌 바이트를 **소비하고 버린다.** 그래서 "먼저 읽는
쪽이 이긴다" 를 조정하는 것만으로는 풀리지 않는다. 어느 쪽이 이기든 반대쪽이 굶는다.

**해법은 호스트가 연 보율로 주인을 가르는 것이다.** 다른 보드에서 이미 쓰던 방식이다
(`stm32h7-gfx/firmware/stm32h7-lvgl` 의 `cmd_task.c` / `cli_mgr.c`).

| 호스트가 연 보율 | `usbGetType()` | CDC 의 주인 |
|---|---|---|
| **115200** | `USB_CON_CLI` | CLI. `cmd` 는 물러난다 |
| 그 외 (툴은 921600) | `USB_CON_CDC` | `cmd`. CLI 는 UART1 로 물러난다 |

- 판정: `cdc.c` 의 `cdcGetType()` — 앱은 `CDC_SET_LINE_CODING` 에서 `cdc_type` 을
  갱신하고, 부트로더는 `tud_cdc_n_get_line_coding()` 을 읽는다
- 소비 측: `cli_mgr.c` 는 `USB_CON_CLI` 일 때만 CDC 를 잡고,
  `cmd_task.c` 의 `cmdChIsEnabled()` 는 그때 CDC 채널을 건너뛴다
- HID 는 전용 채널이라 이 판정과 무관하게 항상 동작한다

호스트 도구(`tools/download/download.py`)는 `CMD_BAUD = 921600` 으로 연다.
USB CDC 라 실제 전송 속도와는 무관한 값이다.

`usb info` 에 `CDC Baud` / `CDC Owner` 를 넣어 지금 누가 쥐고 있는지 바로 보인다.

**호스트 쪽도 한 줄 굳혔다.** 115200 으로 열면 CLI 가 입력을 그대로 에코하는데,
그 에코가 `STX0 STX1` 로 시작하니 응답으로 오인된다(에러 0 으로 조용히 성공). 
`cmdproto.py` 가 `type == PKT_TYPE_RESP` 이고 `cmd` 가 일치할 때만 응답으로 받도록 했다.

### 6. `uart_hw_tbl` 항목 수가 `UART_MAX_CH` 와 어긋나 있었다

`HW_UART_MAX_CH` 를 3 → 4 로 올리면서(가상 CLI 채널 추가) 초기화 항목이 3개로 남아
있었다. 나머지는 0 으로 채워져 채널 이름이 NULL 이 된다. 4개로 맞췄다.

### 7. cmd 채널의 CLI 가 반복 명령에서 영원히 돌았다

`usb info` 처럼 `while (cliKeepLoop())` 로 도는 명령은 CLI 포트에 입력이 들어와야
빠져나온다. cmd 채널의 가상 CLI 에는 **키를 눌러줄 사람이 없다.** 그러면
`cliMain()` 이 반환하지 않고, `cmd_boot.c` 의 300ms 시간 예산도 소용이 없다.
게다가 그 안의 `delay()` 가 `cliLoopIdle() -> moduleUpdate()` 를 부르므로 다음 요청이
재진입 처리되어 출력이 뒤섞인다. 실제로 한 번 걸리면 그 뒤 **모든 커맨드가 죽었다.**

세 군데를 고쳐야 완전히 잡힌다.

1. **`drv_cli.c` 의 `drvCliUpdate()`** — 명령 시작 후 200ms 가 지나면 개행 하나를
   RX 큐에 넣어 "키가 눌린" 것으로 만든다. 반복 명령은 화면 한 장을 뱉고 빠져나온다.
   이 함수는 `cliLoopIdle() -> moduleUpdate()` 경로에서 불려야 한다. `cliMain()` 안에
   갇혀 있는 동안 끊어줄 수 있는 유일한 지점이기 때문이다.
2. **`cmd_task.c` 의 재진입 가드** — 처리 도중 다음 패킷을 또 처리하지 않는다.
   단 `drvCliUpdate()` 는 가드보다 **먼저** 부른다.
3. **`cli_mgr.c` 가 `is_enable == false` 면 포트 전환도 멈춘다** ← 진짜 원인

3번이 핵심이었다. `cliMgrEnable(false)` 는 `cliMain()` 만 막고 있었고, 포트 자동
전환은 그대로 돌았다. `cmd_boot.c` 가 CLI 포트를 가상 채널로 돌려놓아도 다음
`moduleUpdate()` 에서 `cli_mgr` 이 **원래 포트로 되돌려 버린다.** 그러면
`cliKeepLoop()` 은 엉뚱한 포트의 입력을 보게 되고, 우리가 가상 채널에 넣어준
개행은 영원히 읽히지 않는다.

디버그 로그가 그대로 말해줬다 — `run=0`(끊기는 이미 실행됨) 인데 `avail=1`(넣어둔
개행이 그대로 남아 있음) 이면서 루프는 계속 돌고 있었다.

### 8. 웹 CLI 출력이 1KB 에서 잘렸다

`drv_cli.c` 의 출력 버퍼가 1KB 였고, 넘치면 **조용히 버렸다.** `log boot` 이나
`boot log` 는 수 KB 라 뒷부분이 통째로 사라진다. 호스트는 그게 전부인 줄 안다.

세 가지를 함께 고쳤다.

- 버퍼를 4KB 로 키우고, 꼬리에 `... (출력이 잘렸다)` 를 넣을 자리를 남겼다.
  **조용한 잘림을 없애는 것**이 크기보다 중요하다
- 응답을 512바이트 조각으로 나눈다. 응답 형식은 `[0] = more, [1:] = 텍스트` 이고,
  호스트는 more 가 0 이 될 때까지 `BOOT_CMD_CLI_MORE (0x0011)` 로 가져간다
- 웹 CLI 패널과 호스트 도구가 그 루프를 돈다

실측 : `boot log` 2601B 를 조각 6개로 0.13초에 받는다.

### 9. 웹페이지가 응답을 기다리는 동안 탭이 바뀌면 터졌다

```
Cannot set properties of null (setting 'innerHTML')
Offset is outside the bounds of the DataView
```

패널의 `refresh()` 는 비동기다. HID 로 로그 30여 건을 읽는 동안 사용자가 탭을
바꾸면 `$('panel').innerHTML` 이 갈리고, 돌아왔을 때 쓰려던 요소가 없다.
`ctx.isActive(id)` 를 만들어 `await` 뒤마다 확인한다.

두 번째 오류는 **오류 응답을 검사 없이 파싱**해서 났다. 길이 0 인 페이로드에
`getUint16(0)` 을 부르면 저 메시지가 나온다. `r.err` 를 먼저 보고, 파서마다
최소 길이를 확인하게 했다.

## 앱과 부트로더의 동작 차이

`cmd_boot.c` 는 **한 파일을 공유**하고 `HW_DEV_MODE` 로만 갈린다.

| 커맨드 | 부트로더 | 앱 |
|---|---|---|
| `FW_BEGIN`~`FW_END` | 슬롯에 기록 | **동일** |
| `FW_UPDATE` | 슬롯 → FIRM 복사 후 점프 | `resetToUpdate()`. 적용은 다음 부팅에 |
| `FW_JUMP` | 앱으로 점프 | `resetToBoot()` — 부트로더로 재진입 |

앱이 직접 적용할 수 없는 이유는 단순하다. 자기가 실행 중인 뱅크1 을 지울 수 없다.

## 검증

### 실기

```
$ usb desc                       (앱 CDC CLI)
wTotalLength   : 107
bNumInterfaces : 3
NumClasses     : 2
cdc class id   : 0
hid class id   : 1
```

```
hidapi 열거
  0xcafe 0xb003  usage_page 0xFF00  usage 1  'STM32H5-W6300-FW'  interface 2
```

| 항목 | 결과 |
|---|---|
| 앱 HID `BOOT_CMD_INFO` | `mode=1` (APP), 이름/버전 정상 |
| 앱 HID CLI (`boot info` / `wiznet info` / `usb desc`) | OK |
| 앱 HID `FW_JUMP` → 부트로더 재진입 | OK |
| 앱 HID CLI 반복 명령(`usb info`) | 0.21s 에 화면 한 장, 이후 명령도 정상 |
| 앱 CDC @115200 | CLI. `cmd` 는 타임아웃(의도된 동작) |
| 앱 CDC @921600 | cmd. **139 KB/s**, 슬롯 스테이징 + 검증 OK |
| 부트로더 CDC @115200 | CLI (SWD CLI 가 조용해지는 것으로 확인) |
| 부트로더 CDC @921600 | 다운로드 130 KB/s |

### 자동 시험

```
호스트 유닛   44 passed
타깃 통합     22 passed  (4분 11초)
```

## 실측 크기

| | 이전 | 이후 | 증가 |
|---|---|---|---|
| 앱 | 148,976 B (32.7%) | 165,016 B (36.2%) | +16,040 B |
| 부트로더 | 83,368 B (64.6%) | 83,680 B (64.9%) | +312 B |

앱 증가분은 composite 빌더 + HID 클래스 + cmd 모듈이다. 부트로더 증가분은
CDC 주인 판정과 CLI 루프 끊기뿐이다.

빌드 경고는 세 빌드(앱 `-Wall`, 부트로더 `-Wall`, 호스트 테스트 `-Wall -Wextra`)
모두 **0** 이다.
