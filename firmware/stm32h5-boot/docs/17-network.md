# 17. 네트워크 상태 (BOOT_CMD_NET)

## 목적

웹페이지가 보드의 IP/MAC 과 링크 상태를 보게 한다. 보드가 IP 를 받으면 거기서
**보드 자체 웹페이지로 넘어갈 수 있다.**

`14-roadmap.md` B 의 첫 단계다. LAN 스캔은 이 커맨드에 항목을 더해 붙인다.

## 대상 파일

```
src/ap/modules/cmd/process/cmd_boot.c   BOOT_CMD_NET (0x0013)
src/hw/driver/wiznet/wiznet.c           wiznetGetInfo() 의 dhcp 필드 채우기
web/boot.js                             parseNet()
web/panels/board.js                     네트워크 표 · 보드 웹페이지 열기
```

## 응답

```c
typedef struct
{
  uint8_t  is_valid;      // 0 이면 이 쪽에 이더넷이 없다(부트로더)
  uint8_t  is_link;
  uint8_t  is_dhcp;
  uint8_t  is_ip_get;
  uint8_t  mac[6];
  uint8_t  rsv[2];
  uint8_t  ip[4];
  uint8_t  sn[4];
  uint8_t  gw[4];
  uint8_t  dns[4];
} __attribute__((packed)) boot_net_t;      // 32B
```

**이더넷이 없는 쪽도 응답한다.** `is_valid = 0` 으로 알려주면 호스트가
"지원 안 함" 과 "통신 실패" 를 구분할 수 있다. 커맨드 자체를 빼면 둘 다
타임아웃으로 보여 구분이 안 된다.

## 함정

### IP 가 있다고 다 받은 IP 가 아니다 ← 실제로 헷갈렸다

`wiznet.c` 의 `net_info` 에는 컴파일 시점 기본값이 박혀 있다.

```c
static wiz_NetInfo net_info = { .ip = {172, 30, 1, 57}, ... };
```

케이블이 빠져 있어도 이 값이 그대로 읽힌다. 실제로 조회했을 때
`link=0, ip_get=0` 인데 `IP 172.30.1.57` 이 나와서, DHCP 로 받은 주소인 줄 알았다.

`is_ip_get` 이 거짓이면 화면에 **(기본값, 할당 전)** 을 붙이고, "보드 웹페이지
열기" 링크도 감춘다. 있지도 않은 주소로 안내하면 안 된다.

### wiznetGetInfo() 가 dhcp 를 채우지 않고 있었다

`wiznet_info_t` 에 자리는 있는데 복사가 빠져 있었다. 호출자는 스택 쓰레기값을
읽는다. 처음 조회했을 때 `dhcp=0` 이 나와서 드러났다 - DHCP 를 쓰고 있는데도.

구조체에 자리만 있고 채우지 않는 필드는, 없느니만 못하다. 조용히 틀린 값을
주기 때문이다.

## 보드 웹페이지로 넘어가기

IP 를 받으면 `보드 웹페이지 열기` 가 나타난다. 새 탭으로 `http://<보드IP>/` 를
연다.

이 페이지(GitHub Pages)는 HTTPS 라서 `http://보드IP` 로 `fetch`/`WebSocket` 을
걸 수 없다(mixed content 차단). 다만 **최상위 이동은 허용**되므로 새 탭으로 여는
것은 된다. 그 뒤로는 보드 자체 웹서버가 상대한다 - `14-roadmap.md` C 의 구도가
그대로 성립한다.

## 다음

- LAN 스캔. 브라우저는 원시 스캔을 못 하므로 **USB 로 붙은 보드가 스캔해서
  목록을 돌려준다**(`14-roadmap.md` B). `BOOT_CMD_NET` 에 스캔 시작/결과 조회
  커맨드를 더한다
- 보드 자체 웹서버(`14-roadmap.md` C)

## 검증

```
valid=1 link=0 dhcp=1 ip_get=0   IP 172.30.1.57  GW 172.30.1.254
```

케이블을 뽑은 상태라 링크가 없고 IP 도 못 받았다. 값 자체는 펌웨어 기본값이며,
화면에는 그렇게 표시된다. **케이블을 꽂은 상태의 확인은 아직 못 했다.**
