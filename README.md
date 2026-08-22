# stm32h5-w6300

STM32H563 + W6300 이더넷 보드. 부트로더와 앱, 그리고 브라우저에서 보드를 다루는
웹페이지로 이루어져 있다.

## 웹페이지

**https://chcbaram.github.io/stm32h5-w6300/**

Chrome / Edge 에서 연다. WebHID 로 USB 에 붙은 보드를 잡는다(Safari·Firefox 미지원).

| 패널 | 하는 일 |
|---|---|
| 펌웨어 | 슬롯 상태, `.bin` 업데이트, 부트 이벤트 로그, 펌웨어 실행 / 부트로더 재진입 |
| 보드 정보 | 플래시 배치, 네트워크(IP·MAC), 보드 시각과 PC 동기화, LAN 의 보드 스캔 |
| CLI | 보드 CLI 를 그대로 |

부트로더와 앱이 **같은 VID/PID 로 열거**되고 커맨드 셋도 같다. 어느 쪽에 붙었는지는
페이지가 알아서 표시하고, 그에 맞는 항목만 보여준다.

## 구성

```
index.html, web/            웹페이지 (GitHub Pages 로 그대로 배포된다)
firmware/stm32h5-boot/      부트로더 128KB — UF2 / CDC / HID 업데이트, 슬롯 롤백
firmware/stm32h5-fw/        앱 — 이더넷, CLI, HID/CDC/TCP 커맨드 채널
firmware/stm32h5-boot/docs/ 설계 문서. 시작은 STATUS.md
```

## 펌웨어 업데이트 경로

| 경로 | 속도 | 비고 |
|---|---|---|
| UF2 드래그&드롭 | — | 아무것도 설치 안 된 PC 에서도 된다. 부트로더 전용 |
| USB CDC | 140 KB/s | `tools/download/download.py` |
| USB HID | 35 KB/s | 웹페이지. 드라이버 설치 불필요 |
| 이더넷 TCP | 90 KB/s | `download.py --tcp <IP>` |

전부 **같은 슬롯에 같은 태그 포맷**으로 기록되므로, 어느 경로로 넣든 슬롯 핑퐁과
자동 롤백이 동일하게 동작한다.

## 보드 찾기

```bash
python3 firmware/stm32h5-fw/tools/net/discover.py
```

웹페이지의 `LAN 의 보드 → 스캔` 은 같은 일을 **보드가** 한다. 브라우저는 네트워크를
직접 훑을 수 없기 때문이다.

## 문서

설계 결정과 실기에서 밟은 함정은 [firmware/stm32h5-boot/docs](firmware/stm32h5-boot/docs/) 에
단계별로 정리되어 있다. [STATUS.md](firmware/stm32h5-boot/docs/STATUS.md) 부터 보면 된다.
