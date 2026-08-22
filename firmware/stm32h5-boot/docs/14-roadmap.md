# 14. 남은 설계 (로드맵)

확정했지만 아직 구현하지 않은 것들. 결정 근거를 남겨 나중에 다시 고민하지 않게 한다.

---

## A. 앱에 HID 추가 — ST composite 로 간다  ✅ 완료

> **구현 완료. 실제 결과와 함정은 `15-app-hid.md` 를 본다.**
> 아래는 착수 전에 적어둔 결정과 근거로, 그대로 유효하다.
> 다만 엔드포인트 예산은 실제로 다시 잡았다(592B → 464B). MSC 가 없기 때문이다.

### 결정

앱은 **ST USB Device Library 를 유지**하고 composite(CDC + HID)로 확장한다.
TinyUSB 로 교체하지 않는다.

### 근거

처음엔 TinyUSB 교체를 권했다가 **convex 를 확인하고 뒤집었다.**
`convex/firmware/convex-qmk` 가 이미 ST 스택으로 CDC+HID composite 를 하고 있고,
내가 "로컬에 없다" 고 판단한 파일들이 **거기 전부 있다.** 전부 ST 공식 파일이다.

| 필요한 것 | convex 위치 | 정체 |
|---|---|---|
| composite | `src/hw/driver/usb/usb_cmp/usbd_cmp.c` (2012줄) | ST `usbd_composite_builder.c` |
| HID 클래스 | `src/hw/driver/usb/usb_hid/usbd_hid.c` (1274줄) | ST 공식 HID 클래스 |
| 디스크립터 | `usb_cmp/usbd_desc.c` | |
| 설정 | `usbd_conf.h` | `USE_USBD_COMPOSITE`, `USBD_MAX_NUM_INTERFACES 15` |

등록 방식:

```c
USBD_Init(&USBD_Device, &CMP_Desc, DEVICE_FS);
USBD_RegisterClassComposite(&USBD_Device, USBD_HID_CLASS, CLASS_TYPE_HID, hid_ep_tbl);
USBD_RegisterClassComposite(&USBD_Device, USBD_CDC_CLASS, CLASS_TYPE_CDC, cdc_ep_tbl);
USBD_CDC_RegisterInterface(&USBD_Device, &USBD_CDC_fops);
USBD_Start(&USBD_Device);
```

앱의 USB 스택을 그대로 두므로 회귀 범위가 좁다. 스택을 갈면 인터럽트·클럭·PMA 를
전부 다시 검증해야 한다. `UsbMode_t` 에 `USB_CMP_MODE` 를 더하는 형태라 기존
`usbBegin(USB_CDC_MODE)` 구조와도 맞물린다.

### 주의할 점

- convex 는 **OTG_FS(H7)**, 우리는 **USB_DRD_FS(H5)** 다. 엔드포인트 테이블
  (`hid_ep_tbl`/`cdc_ep_tbl`)을 H5 PMA(2KB)에 맞게 다시 잡아야 한다.
  부트로더에서 계산해둔 값을 그대로 쓰면 된다:
  BTABLE 64 + EP0 128 + notif 16 + CDC 128 + HID 128 = **592B**
- convex 앱 HID 는 **QMK 키보드용**이라 usage page 가 다르다. 웹 업데이터가 잡으려면
  부트로더와 동일한 **vendor-defined usage page(0xFF00)** 리포트 디스크립터를 써야 한다.
- `cdc.c` 의 공개 인터페이스 8개는 두 프로젝트에서 이미 동일하므로
  `uart.c` 와 `cli_mgr.c` 는 건드리지 않는다.

### 그다음  ✅ 완료

앱에 `drv_hid.c`(64바이트 리포트 규약은 동일, `tud_hid_report()` 호출부만 ST API 로)
와 `drv_cli.c` 를 붙였다. **웹에서 앱 CLI 가 그대로 돈다.**

---

## B. 웹에서 네트워크 보드 찾기 — 보드가 스캔한다  ✅ 완료

> 구현 완료. 결과와 규약은 `18-lan-scan-tcp-ota.md` 를 본다.

### 결정

브라우저가 LAN 을 스캔하지 않는다. **USB 로 붙은 보드가 스캔해서 목록을 돌려준다.**

### 근거

브라우저 페이지는 원시 네트워크 스캔을 할 수 없다(ICMP·UDP 브로드캐스트·ARP 불가).
게다가 GitHub Pages 는 HTTPS 라서 `https://` 페이지에서 `http://192.168.x.x` 로
나가는 `fetch`/`XHR`/`WebSocket` 은 **mixed content 로 차단**된다.

그런데 스캔이 애초에 필요 없다. 보드가 이미 USB 로 붙어 있으므로:

- HID 로 물어보면 보드가 **자기 IP 를 정확히** 알려준다 (추측이 아니라 확정값)
- 다른 보드는 **USB 에 붙은 보드가 W6300 으로 스캔**해서 목록을 돌려준다.
  브라우저가 못 하는 일을 보드가 대신한다.

### 흐름

```
GitHub Pages ──USB(HID)──▶ 보드 A
                            ├─ "내 IP는 172.30.1.57"
                            └─ "LAN에 .58, .61도 있다"     ← 보드가 스캔
      │
      └─ 목록에서 선택 → window.open('http://172.30.1.58/')
                              │
                       보드 B 자체 웹서버 (same-origin http)
                       → fetch/WebSocket 자유롭게 사용
```

**최상위 페이지 이동(링크·`window.open`)은 https → http 도 허용**된다.
mixed content 는 서브리소스와 fetch/WS 만 막는다. 주소창 경고는 뜨지만 열린다.

### 필요한 것

- 앱에 `BOOT_CMD_NET_INFO`(자기 IP/MAC/DHCP 상태), `BOOT_CMD_NET_SCAN`(LAN 목록)
- 웹에 네트워크 패널 (`web/panels/network.js`)

---

## C. 보드 자체 웹서버  ✅ 완료

> 구현 완료. WebSocket 대신 HTTP POST 로 갔다. 이유와 결과는 `19-board-webserver.md`.

### 결정

보드가 자기 웹페이지를 서빙한다. 소스는 **가능한 한 GitHub Pages 와 공유**하고,
분리가 필요한 기능만 나눈다.

### 근거

same-origin http 라 mixed content 제약이 사라지고 `fetch`/`WebSocket` 을 자유롭게
쓸 수 있다. 펌웨어 업데이트와 테스트를 전부 네트워크로 할 수 있다.

패널 구조가 이미 이걸 염두에 둔 형태다.

```
web/proto.js       Channel 추상 + HidChannel   ← WsChannel 을 여기 추가
web/boot.js        커맨드 셋 (공용)
web/panels/*.js    화면. 타깃별로 포함할 패널만 고른다
```

- **GitHub Pages 빌드**: `HidChannel`, 전 패널
- **보드 빌드**: `WsChannel`, 크기에 맞는 패널만. gzip 후 10~30KB 예상

### 필요한 것

- W6300 위 최소 HTTP/WebSocket 서버 (앱이 이미 소켓으로 텔넷 CLI 를 한다)
- 페이지 저장 위치: **뱅크1 예약 448KB**(0x08090000). littlefs 또는 단순 blob
- 빌드 후처리: 선택한 패널을 묶고 gzip 해서 펌웨어에 넣기

---

## D. NVS 가상 EEPROM

`0x081E4000`, 112KB. 기존 `nvs.h` 의 `nvsSet/nvsGet/nvsIsExist` 를 구현한다.
16B 정렬 레코드 append + 섹터 full 시 컴팩션(쿼드워드 재기록 없음).

이번에 만든 `boot_log.c` 가 같은 패턴의 축소판이라 그대로 확장하면 된다.

littlefs 는 쓰지 않는다. STM32H5 는 프로그램된 쿼드워드를 다시 못 쓰는데 littlefs 는
블록 안에서 조금씩 append 하는 구조라 궁합이 나쁘고, 코드도 15~20KB 로 크다.
큰 read-mostly 데이터(폰트/이미지/웹페이지)가 필요해지면 그때 **뱅크1 예약 영역**에
얹는다.

---

## E. 이더넷 OTA  ✅ 완료

> 구현 완료. `drv_tcp.c` 하나로 끝났다. `18-lan-scan-tcp-ota.md` 를 본다.

앱이 `bootGetWriteSlot()` 으로 주소를 얻어 슬롯에 직접 기록하고 `resetToUpdate()`.
부트로더 쪽 코드는 이미 완성되어 있다. `cmd_udp.c` 드라이버만 추가하면 CDC/HID 와
동일한 커맨드 셋을 재사용한다(apm32e103-kit 에 선례가 있다).

---

## F. 펌웨어 압축 (보류)

슬롯에 압축 이미지를 받으면 가용 앱 크기가 늘어난다. heatshrink 면 448KB 슬롯이
~750KB 앱을 담는다. 하지만 보류한다.

- 가용 앱 크기가 **가변**이 된다. 이미 압축된 자산이 들어가면 비율이 1.0 에 수렴해
  슬롯을 넘긴다. 어제 되던 빌드가 오늘 안 되는데 그게 현장에서 드러나면 최악이다.
- **슬롯 판별 규칙이 깨진다.** 핑퐁은 "FIRM 의 `(fw_size, fw_crc)` 와 일치하는 슬롯
  = 백업본" 인데, 슬롯이 압축이면 FIRM(비압축)과 비교할 수 없다.
- 현재 앱은 148KB. 448KB 슬롯은 3배 여유다. 필요해지면 그때 한다.

**나중에 무손실로 켜는 방법**: UF2 페이로드 오프셋 0 의 첫 4바이트로 판별한다.
비압축 이미지는 첫 워드가 스택 포인터(`0x200A0000`)이므로 `"CFW1"` 같은 컨테이너
매직과 절대 충돌하지 않는다. **부트로더만 바꾸면 되고 기존 UF2 는 그대로다.**
도입 기준: 앱 이미지가 슬롯의 80%(≈358KB)에 도달했을 때.
