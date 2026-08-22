//-- 부트로더/앱 공용 cmd 패킷 프로토콜 (WebHID)
//
//   펌웨어의 src/hw/driver/cmd.c 와 동일한 포맷이다.
//     STX0(0x02) STX1(0xFD) type cmd_l cmd_h err_l err_h len_l len_h [data] checksum
//     checksum = (~sum(header+data)) + 1
//
//   HID 는 스트림이 아니라 64바이트 고정 리포트라, 펌웨어(drv_hid.c)와 같은 규약으로
//   조각낸다.  리포트[0] = 유효 바이트 수, 리포트[1:] = 페이로드
//
//   이 파일은 전송/프로토콜만 담당한다. 화면은 panels/*.js 가 맡는다.
//   앱(stm32h5-fw)에 HID 가 붙으면 같은 채널로 보드 정보/테스트 커맨드를 태운다.
//
export const RPT = 64;
export const PAYLOAD = RPT - 1;

export const PKT_TYPE_CMD  = 0x00;
export const PKT_TYPE_RESP = 0x01;

export function buildPacket(cmd, data = new Uint8Array(0), type = PKT_TYPE_CMD) {
  const p = new Uint8Array(9 + data.length + 1);
  const dv = new DataView(p.buffer);
  p[0] = 0x02; p[1] = 0xFD; p[2] = type;
  dv.setUint16(3, cmd, true);
  dv.setUint16(5, 0, true);
  dv.setUint16(7, data.length, true);
  p.set(data, 9);
  let sum = 0;
  for (let i = 0; i < 9 + data.length; i++) sum += p[i];
  p[9 + data.length] = ((~sum) + 1) & 0xFF;
  return p;
}

//-- 전송계층 공통 규약.
//
//   펌웨어의 cmd.c 가 전송계층과 무관하게 설계된 것과 같은 이유로, 웹도 전송을
//   갈아끼울 수 있게 해둔다. 지금은 HID 하나지만 나중에 W6300 네트워크(WebSocket)가
//   붙는다.
//
//   주의: GitHub Pages 는 https 라서 브라우저가 http 로의 접속을 mixed content 로
//         차단한다. 그래서 네트워크 전송은 **보드가 직접 서빙한 페이지**에서만
//         쓴다. 그때는 같은 출처라 제약이 없다.
//
export class Channel {
  async send(bytes)        { throw new Error('not implemented'); }
  takeRx()                 { throw new Error('not implemented'); }  // 받은 바이트를 비우며 돌려준다
  detach()                 {}

  async request(cmd, data, timeoutMs = 4000) {
    this._rx = new Uint8Array(0);
    await this.send(buildPacket(cmd, data || new Uint8Array(0)));

    const t0 = performance.now();
    for (;;) {
      this._append(this.takeRx());
      for (let i = 0; i + 9 <= this._rx.length; i++) {
        if (this._rx[i] !== 0x02 || this._rx[i + 1] !== 0xFD) continue;
        const dv = new DataView(this._rx.buffer, this._rx.byteOffset + i);
        const err = dv.getUint16(5, true);
        const len = dv.getUint16(7, true);
        if (this._rx.length - i < 9 + len + 1) break;
        return { err, data: this._rx.slice(i + 9, i + 9 + len) };
      }
      if (performance.now() - t0 > timeoutMs)
        throw new Error(`cmd 0x${cmd.toString(16).padStart(4, '0')} 응답 없음`);
      await new Promise(r => setTimeout(r, 2));
    }
  }

  _append(chunk) {
    if (!chunk || !chunk.length) return;
    const n = new Uint8Array((this._rx?.length || 0) + chunk.length);
    if (this._rx) n.set(this._rx);
    n.set(chunk, this._rx?.length || 0);
    this._rx = n;
  }
}

export class HidChannel extends Channel {
  constructor(device) {
    super();
    this.dev = device;
    this.pending = new Uint8Array(0);
    this._rx = new Uint8Array(0);
    this._onReport = (e) => {
      const d = new Uint8Array(e.data.buffer);
      const n = d[0];
      // 리포트[0] 이 유효 바이트 수다. HID 는 항상 64바이트를 꽉 채워 보내므로
      // 이 값이 없으면 패딩과 실제 데이터를 구분할 수 없다(펌웨어 drv_hid.c 와 동일 규약).
      if (n > 0 && n <= PAYLOAD) {
        const m = new Uint8Array(this.pending.length + n);
        m.set(this.pending); m.set(d.slice(1, 1 + n), this.pending.length);
        this.pending = m;
      }
    };
    device.addEventListener('inputreport', this._onReport);
  }

  detach() { this.dev.removeEventListener('inputreport', this._onReport); }

  takeRx() {
    const out = this.pending;
    this.pending = new Uint8Array(0);
    return out;
  }

  async send(bytes) {
    for (let i = 0; i < bytes.length; i += PAYLOAD) {
      const c = bytes.slice(i, i + PAYLOAD);
      const rpt = new Uint8Array(RPT);
      rpt[0] = c.length;
      rpt.set(c, 1);
      await this.dev.sendReport(0, rpt);
    }
  }
}

export const str32 = (u8, off) =>
  new TextDecoder().decode(u8.slice(off, off + 32)).split('\0')[0].trim();


//-- 보드가 서빙한 페이지에서 쓰는 전송.
//
//   POST /cmd 한 번에 요청 패킷을 싣고 응답 패킷을 받는다. cmd 가 원래
//   요청/응답이라 그대로 맞아떨어진다.
//
//   WebSocket 을 쓰지 않은 이유는 펌웨어 net_http.c 의 주석에 적어뒀다 -
//   업그레이드 핸드셰이크와 프레이밍이 MCU 쪽에 200줄쯤 더 붙는데, 얻는 것은
//   서버 푸시뿐이고 지금은 필요가 없다.
//
export class HttpChannel extends Channel {
  constructor(base = '') {
    super();
    this.base = base;
    this._rx = new Uint8Array(0);
  }

  //   Channel.request() 의 폴링 루프를 쓰지 않는다. POST 는 한 번에 끝나므로
  //   응답을 바로 파싱하는 편이 빠르고 단순하다.
  async request(cmd, data, timeoutMs = 8000) {
    const ac = new AbortController();
    const timer = setTimeout(() => ac.abort(), timeoutMs);

    let res;
    try {
      res = await fetch(`${this.base}/cmd`, {
        method: 'POST',
        body: buildPacket(cmd, data || new Uint8Array(0)),
        signal: ac.signal,
      });
    } catch (e) {
      throw new Error(`cmd 0x${cmd.toString(16).padStart(4, '0')} 전송 실패: ${e.message}`);
    } finally {
      clearTimeout(timer);
    }

    if (!res.ok) throw new Error(`HTTP ${res.status}`);

    const rx = new Uint8Array(await res.arrayBuffer());
    for (let i = 0; i + 9 <= rx.length; i++) {
      if (rx[i] !== 0x02 || rx[i + 1] !== 0xFD) continue;
      const dv = new DataView(rx.buffer, rx.byteOffset + i);
      const err = dv.getUint16(5, true);
      const len = dv.getUint16(7, true);
      if (rx.length - i < 9 + len + 1) break;
      return { err, data: rx.slice(i + 9, i + 9 + len) };
    }
    throw new Error(`cmd 0x${cmd.toString(16).padStart(4, '0')} 응답을 못 읽었다`);
  }

  async send()  { throw new Error('HttpChannel 은 request() 만 쓴다'); }
  takeRx()      { return new Uint8Array(0); }
}

//-- 이 페이지를 보드가 서빙했는가.
//
//   http 로 열렸고 호스트가 IPv4 면 보드로 본다. GitHub Pages(https)와
//   개발용 localhost 는 여기에 걸리지 않으므로 HID 로 간다.
//
export function isBoardHosted() {
  return location.protocol === 'http:' &&
         /^\d{1,3}(\.\d{1,3}){3}$/.test(location.hostname);
}
