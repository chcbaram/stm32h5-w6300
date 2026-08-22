# 19. 보드 자체 웹서버

## 목적

보드가 자기 웹페이지를 서빙한다. **같은 출처(http)** 라 mixed content 제약이
사라지고, 브라우저에서 네트워크만으로 보드를 다룰 수 있다 (`14-roadmap.md` C).

## 대상 파일

```
src/ap/modules/net/net_http.{c,h}      최소 HTTP 서버
src/ap/modules/net/web_page.c          자동 생성. gzip 된 페이지
tools/web/gen_web.py                   번들 + gzip + C 배열 생성
web/proto.js                           HttpChannel, isBoardHosted()
index.html                             보드가 서빙했으면 자동 연결
```

## 설계 결정과 근거

### WebSocket 대신 HTTP POST ← 로드맵에서 바꿨다

로드맵에는 WebSocket 이라고 적어뒀는데 **POST 로 바꿨다.**

- `cmd` 는 원래 **요청/응답**이다. POST 한 번에 그대로 맞아떨어진다
- WebSocket 은 업그레이드 핸드셰이크(SHA1+base64)와 프레임 마스킹 해제가 필요하다.
  MCU 에 200줄쯤이 더 붙는데, 얻는 것은 **서버 푸시**뿐이고 지금은 쓸 데가 없다

```
GET  /              gzip 된 페이지
GET  /web/app.js    gzip 된 모듈 묶음
POST /cmd           본문 = cmd 패킷 원본, 응답 = 응답 패킷 원본
```

푸시가 필요해지면(로그 스트리밍 같은) 그때 WebSocket 을 더하면 된다.

### 페이지는 앱 바이너리에 넣는다

뱅크1 예약 448KB(`0x08090000`)에 두는 방안이 있었지만 **앱에 그냥 넣었다.**

- gzip 후 18KB. 앱이 192KB/445KB 라 부담이 없다
- 별도 굽기 단계가 없다. **OTA 로 펌웨어를 올리면 페이지도 같이 간다**
- 예약 영역은 이미지·폰트처럼 커지는 것이 생길 때 쓴다

### 모듈을 하나로 묶는다 ← 소켓이 모자란다

W6300 은 소켓이 8개인데 0~4 를 telnet/DHCP/SNTP/cmd TCP/discovery 가 쓴다.
HTTP 에 남는 것은 **5, 6, 7 셋뿐**이다.

브라우저는 파일마다 연결을 연다. 처음에 파일이 여섯이었고(index.html + 모듈 5개)
동시 요청을 던져보니 **셋은 연결조차 못 했다.**

```
000 0B  /web/panels/firmware.js
000 0B  /web/panels/board.js
000 0B  /
200 ...  /web/panels/cli.js
200 ...  /web/proto.js
200 ...  /web/boot.js
```

keep-alive 를 넣었지만 그것만으로는 부족했다. 브라우저가 **동시에** 여는 연결에는
소용이 없기 때문이다. 그래서 JS 다섯 개를 `app.js` 하나로 묶어 요청을 **둘**로
줄였다. 셋이면 넉넉하다.

묶는 방식은 작은 레지스트리다. 각 모듈을 함수로 감싸 스코프를 지킨다.

```js
const __mods = {};
function __def(name, fn) { __mods[name] = { fn, ex: null }; }
function __req(name) { ... }
```

파일을 그냥 이어붙이면 안 된다. **패널마다 `id`/`title`/`render`/`mount` 가 같은
이름으로 있어서** 서로 덮어쓴다. 함수로 감싸면 이름을 하나도 바꾸지 않아도 된다.

### 브라우저가 서빙 주체를 알아본다

같은 소스가 GitHub Pages 와 보드 양쪽에서 돈다.

```js
export function isBoardHosted() {
  return location.protocol === 'http:' &&
         /^\d{1,3}(\.\d{1,3}){3}$/.test(location.hostname);
}
```

- http 이고 호스트가 IPv4 → 보드가 서빙한 것. `HttpChannel` 로 **자동 연결**하고
  장치 선택창을 띄우지 않는다(고를 것이 하나뿐이다)
- https(GitHub Pages) 나 localhost → `HidChannel`

## 함정

### 여러 줄 import 를 정규식이 놓쳤다 ← 실행 검사가 잡았다

번들러의 import 정규식이 줄 단위(`re.M`)라 이런 것을 놓쳤다.

```js
import { BOOT_CMD, parseVersion, parseInfo, parseLog, EVT_NAME,
         DEV_MODE_BOOT, DEV_MODE_APP, epochToText } from '../boot.js';
```

**문법 검사는 통과했다.** 남은 `import` 가 모듈 문법으로는 합법이기 때문이다.
JavaScriptCore(`osascript -l JavaScript`)로 번들을 **실제로 실행**해서 잡았다.

> 이 환경에 JS 엔진이 없다고 생각했는데 macOS 에 JXA 가 있다. 문법 검사와 간단한
> 실행 검사를 그걸로 한다. 브라우저 확인을 대신하지는 못하지만, 이런 류의 오류는
> 여기서 걸린다.

검사는 모듈별 export 가 제대로 갈렸는지까지 본다.

```
firmware.js [id,modes,mount,onDisconnect,refresh,render,title]
board.js    [id,modes,mount,refresh,render,title]
cli.js      [id,modes,mount,refresh,render,title]
```

### 브라우저 캐시로 페이지가 통째로 죽는다

`index.html` 이 `proto.js` 에서 **새 이름**을 import 하는데 브라우저가 옛
`proto.js` 를 캐시하고 있으면 링크 단계에서 실패한다. 그러면 HTML 만 뜨고
**스크립트가 통째로 안 돈다** - 탭도 없고 버튼도 죽는다. 문법 오류처럼 보이지만
아니다. 하드 리로드로 풀린다.

보드 응답에는 `Cache-Control: no-cache` 를 붙여 이 문제를 줄였다.

### 탭 표시가 사라진다

`buildTabs()` 가 버튼을 다시 만드는데, `current` 가 목록에 그대로 있으면
`selectPanel()` 을 부르지 않아 `on` 클래스가 안 붙는다. 연결 직후에 어느 탭도
선택돼 보이지 않았다. 버튼을 만들 때 표시도 같이 붙인다.

## 검증

```
GET /            200  4800B   (gzip, Content-Encoding: gzip)
GET /web/app.js  200 13059B
GET /nope        404
POST /cmd INFO   err=0 mode=1 STM32H5-W6300-FW

동시 요청 2개 -> 둘 다 200
```

크기 : 원본 51KB -> gzip 18KB. 앱 191,872 B / 445 KB (42.1%)

**브라우저 실동작은 아직 확인하지 못했다.** curl 과 JXA 로 여기까지 왔다.
