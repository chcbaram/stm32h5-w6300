# 18. LAN 스캔과 이더넷 OTA

## 목적

- 같은 네트워크의 보드를 **웹에서** 찾는다
- 펌웨어를 **네트워크로** 올린다 (`14-roadmap.md` B·E)

## 대상 파일

```
src/ap/modules/net/net_disc.{c,h}        UDP 브로드캐스트 비컨
src/ap/modules/cmd/driver/drv_tcp.c      TCP 커맨드 채널
src/ap/modules/cmd/process/cmd_boot.c    BOOT_CMD_NET_SCAN (0x0014)
tools/net/discover.py                    PC 스캔 + 가짜 보드
tools/download/{download.py, cmdproto.py}  --tcp <IP>
web/boot.js, web/panels/board.js         스캔 버튼과 목록
```

## LAN 스캔

### 왜 보드가 스캔하나

브라우저는 원시 네트워크 스캔을 할 수 없다(ICMP·UDP 브로드캐스트·ARP 불가).
그래서 **USB 로 붙은 보드가 대신 훑고 목록만 돌려준다.**

`NU87-TinyDK` 의 `netDiscoverPoll()` + `tools/discover.py` 가 선례다. 다만 거기는
**PC 가** 브로드캐스트를 던지고 보드는 답하기만 한다. 우리는 스캔하는 쪽이 MCU 라
방향이 반대다. 이 차이가 규약 선택을 갈랐다.

| | NU87 | 여기 |
|---|---|---|
| 스캔 주체 | PC | **보드** (와 PC 둘 다) |
| 규약 | 텍스트 한 줄 `NU87 <name> <ver> <mac>` | **60바이트 구조체** |
| 이유 | 파싱하는 쪽이 파이썬 | 파싱하는 쪽이 **MCU** |

응답을 MCU 가 파싱해야 하므로 구조체가 낫다. 받은 것을 그대로 `BOOT_CMD_NET_SCAN`
응답에 실을 수 있어 재포장도 없다. NU87 에서 가져온 것은 **여러 브로드캐스트
주소로 던지기**와 **이름·버전·MAC 을 응답에 싣기** 두 가지다.

### 규약

UDP 포트 **5300**. 질의와 응답이 같은 구조를 쓰고 매직만 다르다.

```c
typedef struct {
  uint32_t magic;        // "BRDQ" 질의 / "BRDR" 응답
  uint8_t  ver;          // 1
  uint8_t  mode;         // HW_DEV_MODE_BOOT / _APP
  uint8_t  rsv[2];
  uint8_t  ip[4];
  uint8_t  mac[6];
  uint8_t  rsv2[2];
  char     name[24];
  char     version[16];
} __attribute__((packed)) net_beacon_t;   // 60B
```

- 소켓 4 는 **항상 열어둔다.** 다른 보드가 물어보면 언제든 답해야 한다
- 브로드캐스트는 **서브넷 브로드캐스트**(`ip | ~sn`)로 보낸다. `255.255.255.255`
  보다 라우팅 사고가 적다
- 자기 자신은 스캔 시작할 때 목록에 **먼저 넣는다.** 칩이 자기 브로드캐스트를
  되받는다는 보장이 없다
- 같은 IP 가 중복으로 오면 무시한다

### 보드가 하나뿐일 때 어떻게 시험했나

`discover.py --serve` 로 **가짜 보드**를 띄웠다. 파이썬이 같은 규약으로 응답하므로
보드 입장에서는 진짜 보드와 구분되지 않는다.

```bash
python3 tools/net/discover.py --serve --name FAKE-BOARD-1 --version V990101R1
```

이러면 양방향이 다 확인된다.

| | 결과 |
|---|---|
| PC 스캔 → 진짜 보드 | `172.30.1.57 APP STM32H5-W6300-FW V260822R1` |
| 보드 스캔(`scan` CLI) → 가짜 보드 | `2 대 (802ms)` 자신 + `FAKE-BOARD-1` |
| `BOOT_CMD_NET_SCAN` (HID) | `124B = 4 + 2×60`, 웹 파서 형식과 일치 |

가짜 보드는 규약 검증에도 쓸모가 있다. 필드 배치가 어긋나면 파이썬 쪽에서
바로 드러난다.

## 이더넷 OTA (TCP)

`cmd.c` 가 전송계층과 무관하게 설계되어 있어서 **여섯 함수만 채우면** CDC/HID 와
같은 커맨드 셋이 네트워크에서 그대로 돈다. **부트로더는 손댄 것이 없다** - 앱이
슬롯에 받아두고 `resetToUpdate()` 하면 다음 부팅에 부트로더가 적용한다.

- 소켓 3, TCP 포트 **5301**
- 소켓에서 바이트 단위로 읽으면 매번 SPI 트랜잭션이 생긴다. `drvTcpUpdate()` 가
  한 번에 끌어와 링버퍼에 쌓고 `cmd.c` 는 거기서 한 바이트씩 가져간다
- 연결이 새로 맺어지면 링버퍼를 비운다. 이전 연결의 찌꺼기가 패킷 파서를 흔든다

```bash
python3 tools/download/download.py <bin> --tcp 172.30.1.57
```

실측 **88~92 KB/s**. 이더넷으로만 업데이트해서 적용·재부팅까지 확인했다.

| 경로 | 속도 |
|---|---|
| USB CDC | 140 KB/s |
| 이더넷 TCP | 90 KB/s |
| USB HID | 35 KB/s |

## 웹

보드 정보 탭에 `LAN 의 보드` 표가 붙었다. 스캔은 800ms 걸리므로 호스트 타임아웃을
8초로 잡았다. 자기 자신은 `이 보드` 로 표시하고, 나머지는 `열기` 로 그 보드의
웹페이지(`http://<IP>/`)를 새 탭에 연다.

펌웨어 탭에는 `펌웨어 실행` / `부트로더로 재진입` 버튼이 생겼다. 둘 다 같은
`FW_JUMP(0x0009)` 다 - 이 커맨드는 양쪽에서 "반대편으로 넘어간다" 는 뜻이라
라벨만 모드에 따라 바뀐다.

## 남은 것

- 보드 자체 웹서버 (`14-roadmap.md` C). 스캔 목록의 `열기` 가 그때 의미를 갖는다
- 스캔 결과에 링크 상태나 가동 시간 같은 것을 더할 수 있다. 지금은 최소한만 싣는다
