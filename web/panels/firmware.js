//-- 펌웨어 갱신 패널 (부트로더 전용)
//
import { BOOT_CMD, parseVersion, parseInfo, parseLog, EVT_NAME } from '../boot.js';

const CHUNK = 512;

export const id    = 'firmware';
export const title = '펌웨어';

export function render() {
  return `
    <div class="card">
      <table>
        <thead><tr><th>영역</th><th>버전</th><th>seq</th><th>크기</th><th>CRC</th></tr></thead>
        <tbody id="fwTbl"><tr><td colspan="5" class="muted">연결 후 표시된다</td></tr></tbody>
      </table>
      <p class="muted small" id="fwSlot"></p>
    </div>

    <div class="card">
      <div class="row">
        <input type="file" id="fwFile" accept=".bin">
        <button id="fwGo" class="primary" disabled>업데이트</button>
      </div>
      <progress id="fwProg" value="0" max="100" hidden></progress>
    </div>

    <div class="card">
      <div class="row"><b>부트 이벤트 로그</b>
        <button id="fwLogGet" class="small">읽기</button></div>
      <table><thead><tr><th>#</th><th>이벤트</th><th>slot</th><th>from</th><th>to</th><th>PC</th></tr></thead>
        <tbody id="fwLog"><tr><td colspan="6" class="muted">읽기를 누른다</td></tr></tbody></table>
    </div>`;
}

let fwData = null;

export function mount(ctx) {
  const { $, log, channel } = ctx;

  $('fwFile').onchange = async (e) => {
    const f = e.target.files[0];
    if (!f) return;
    fwData = new Uint8Array(await f.arrayBuffer());
    log(`파일: ${f.name} (${(fwData.length / 1024).toFixed(1)} KB)`);
    $('fwGo').disabled = !channel();
  };

  $('fwGo').onclick = () => update(ctx).catch(e => log(`실패: ${e.message}`, 'err'));
  $('fwLogGet').onclick = () => readLog(ctx).catch(e => log(`로그 읽기 실패: ${e.message}`, 'err'));
}

export async function refresh({ $, channel }) {
  const ch = channel();
  if (!ch) return;

  const info = parseInfo((await ch.request(BOOT_CMD.INFO)).data);
  const v = parseVersion((await ch.request(BOOT_CMD.VERSION)).data, info.slotMax);

  const row = (label, it) => it.valid
    ? `<tr><td>${label}</td><td>${it.ver || '-'}</td><td>${it.seq}</td>
        <td>${(it.size / 1024).toFixed(0)} KB</td>
        <td>0x${it.crc.toString(16).toUpperCase().padStart(4, '0')}</td></tr>`
    : `<tr><td>${label}</td><td colspan="4" class="muted">(비어 있음)</td></tr>`;

  $('fwTbl').innerHTML = row('FIRM', v.firm) + v.slot.map((s, i) => row(`SLOT${i}`, s)).join('');
  $('fwSlot').textContent =
    `write slot ${v.writeSlot} · pending ${v.pendingSlot} · rollback ${v.rollbackSlot}`;
  $('fwGo').disabled = !fwData;
}

async function update(ctx) {
  const { $, log, channel } = ctx;
  const ch = channel();
  if (!ch || !fwData) return;

  const fw = new Uint8Array(Math.ceil(fwData.length / 16) * 16).fill(0xFF);
  fw.set(fwData);

  $('fwGo').disabled = true;
  $('fwProg').hidden = false;
  $('fwProg').value = 0;

  try {
    const sz = new Uint8Array(4);
    new DataView(sz.buffer).setUint32(0, fw.length, true);

    let r = await ch.request(BOOT_CMD.FW_BEGIN, sz);
    if (r.err) throw new Error(`FW_BEGIN err=0x${r.err.toString(16)}`);
    const slot = r.data[0];
    log(`slot${slot} 에 ${(fw.length / 1024).toFixed(1)} KB 기록`);

    r = await ch.request(BOOT_CMD.FW_ERASE, null, 20000);
    if (r.err) throw new Error(`FW_ERASE err=0x${r.err.toString(16)}`);

    const t0 = performance.now();
    for (let off = 0; off < fw.length; off += CHUNK) {
      const body = fw.slice(off, off + CHUNK);
      const pkt = new Uint8Array(4 + body.length);
      new DataView(pkt.buffer).setUint32(0, off, true);
      pkt.set(body, 4);
      r = await ch.request(BOOT_CMD.FW_WRITE, pkt);
      if (r.err) throw new Error(`FW_WRITE off=${off} err=0x${r.err.toString(16)}`);
      $('fwProg').value = (off + body.length) * 100 / fw.length;
    }
    const dt = (performance.now() - t0) / 1000;
    log(`  기록 완료 ${dt.toFixed(1)}s (${(fw.length / dt / 1024).toFixed(0)} KB/s)`);

    r = await ch.request(BOOT_CMD.FW_END, null, 20000);
    if (r.err) throw new Error(`FW_END err=0x${r.err.toString(16)}`);

    r = await ch.request(BOOT_CMD.FW_VERIFY, new Uint8Array([slot]), 20000);
    if (r.err) throw new Error(`FW_VERIFY err=0x${r.err.toString(16)}`);
    log('  검증 OK', 'ok');

    await ch.request(BOOT_CMD.FW_UPDATE, null, 5000).catch(() => {});
    log('  적용 요청 완료. 부트로더가 FIRM 에 복사한 뒤 재부팅한다.', 'ok');
    log('  장치가 분리된다. 다시 쓰려면 리셋 더블클릭 후 재연결.', 'muted');
  } finally {
    $('fwGo').disabled = false;
  }
}

async function readLog({ $, channel }) {
  const ch = channel();
  if (!ch) return;

  const n = new DataView((await ch.request(BOOT_CMD.LOG_COUNT)).data.buffer).getUint16(0, true);
  if (n === 0) {
    $('fwLog').innerHTML = '<tr><td colspan="6" class="muted">기록 없음</td></tr>';
    return;
  }
  const rows = [];
  for (let i = 0; i < n; i++) {
    const idx = new Uint8Array(2);
    new DataView(idx.buffer).setUint16(0, i, true);
    const r = await ch.request(BOOT_CMD.LOG_READ, idx);
    if (r.err) continue;
    const g = parseLog(r.data);
    const h = (x) => '0x' + x.toString(16).toUpperCase().padStart(4, '0');
    rows.push(`<tr><td>${i}</td><td>${EVT_NAME[g.event] || '?'}</td>
      <td>${g.slot === 0xFF ? '-' : g.slot}</td><td>${h(g.fromCrc)}</td><td>${h(g.toCrc)}</td>
      <td>${g.faultPc ? '0x' + g.faultPc.toString(16).padStart(8, '0') : '-'}</td></tr>`);
  }
  $('fwLog').innerHTML = rows.join('');
}
