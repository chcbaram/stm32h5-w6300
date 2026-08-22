//-- 부트로더/앱 공용 cmd 패킷 프로토콜 (WebHID)
//
//   펌웨어의 src/hw/driver/cmd.c 와 동일한 포맷이다.
//     STX0(0x02) STX1(0xFD) type cmd_l cmd_h err_l err_h len_l len_h [data] checksum
//     checksum = (~sum(header+data)) + 1
//
//   HID 는 스트림이 아니라 64바이트 고정 리포트라, 펌웨어(cmd_hid.c)와 같은 규약으로
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

export class HidChannel {
  constructor(device) {
    this.dev = device;
    this.rx = new Uint8Array(0);
    this._onReport = (e) => {
      const d = new Uint8Array(e.data.buffer);
      const n = d[0];
      if (n > 0 && n <= PAYLOAD) this._append(d.slice(1, 1 + n));
    };
    device.addEventListener('inputreport', this._onReport);
  }

  detach() { this.dev.removeEventListener('inputreport', this._onReport); }

  _append(chunk) {
    const n = new Uint8Array(this.rx.length + chunk.length);
    n.set(this.rx); n.set(chunk, this.rx.length);
    this.rx = n;
  }

  async _sendRaw(bytes) {
    for (let i = 0; i < bytes.length; i += PAYLOAD) {
      const c = bytes.slice(i, i + PAYLOAD);
      const rpt = new Uint8Array(RPT);
      rpt[0] = c.length;
      rpt.set(c, 1);
      await this.dev.sendReport(0, rpt);
    }
  }

  async request(cmd, data, timeoutMs = 4000) {
    this.rx = new Uint8Array(0);
    await this._sendRaw(buildPacket(cmd, data || new Uint8Array(0)));

    const t0 = performance.now();
    for (;;) {
      for (let i = 0; i + 9 <= this.rx.length; i++) {
        if (this.rx[i] !== 0x02 || this.rx[i + 1] !== 0xFD) continue;
        const dv = new DataView(this.rx.buffer, this.rx.byteOffset + i);
        const err = dv.getUint16(5, true);
        const len = dv.getUint16(7, true);
        if (this.rx.length - i < 9 + len + 1) break;
        return { err, data: this.rx.slice(i + 9, i + 9 + len) };
      }
      if (performance.now() - t0 > timeoutMs)
        throw new Error(`cmd 0x${cmd.toString(16).padStart(4, '0')} 응답 없음`);
      await new Promise(r => setTimeout(r, 2));
    }
  }
}

export const str32 = (u8, off) =>
  new TextDecoder().decode(u8.slice(off, off + 32)).split('\0')[0].trim();
