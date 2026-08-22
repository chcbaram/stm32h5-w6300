//-- 펌웨어 갱신 패널
//
import { BOOT_CMD, parseVersion, parseInfo, parseLog, EVT_NAME,
         DEV_MODE_BOOT, DEV_MODE_APP, epochToText, ERR_NO_PENDING } from '../boot.js';

const CHUNK = 512;

export const id    = 'firmware';
export const title = '펌웨어';

//-- 부트로더와 앱 양쪽에서 쓴다.
//
//   커맨드 셋이 같기 때문이다. 다른 것은 적용 방식뿐이다. 부트로더는 슬롯을
//   FIRM 에 바로 복사하고 점프하지만, 앱은 자기가 실행 중인 뱅크를 지울 수
//   없으므로 슬롯에 받아두고 리셋한다 - 적용은 다음 부팅에 부트로더가 한다.
export const modes = [DEV_MODE_BOOT, DEV_MODE_APP];

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
        <button id="fwBoot" class="small" hidden></button>
      </div>
      <progress id="fwProg" value="0" max="100" hidden></progress>
    </div>

    <div class="card">
      <div class="row"><b>부트 이벤트 로그</b>
        <button id="fwLogGet" class="small">읽기</button>
        <span class="muted small" id="fwLogCnt"></span></div>
      <!-- 레코드가 최대 512개까지 쌓인다. 높이를 묶고 안에서 스크롤한다. -->
      <div class="tbl-scroll">
        <table><thead><tr>
          <th>#</th><th>시각</th><th>이벤트</th><th>slot</th><th>from</th><th>to</th><th>PC</th>
        </tr></thead>
        <tbody id="fwLog"><tr><td colspan="7" class="muted">읽기를 누른다</td></tr></tbody></table>
      </div>
    </div>`;
}

let fwData = null;
let bootMode = null;      // 마지막으로 확인한 실행 모드

//-- 기록 시각. 보드에 코인셀이 없어 RTC 를 모르는 구간이 있다.
//   그때 펌웨어는 0 을 남기고, 여기서는 '-' 로 보여준다.
//
//   반드시 epochToText() 를 거친다. 보드 시각은 "달력 필드를 UTC 로 간주해 만든
//   epoch" 이라, new Date().getHours() 같은 지역시 접근자로 읽으면 타임존만큼
//   밀린다(KST 면 9시간). 표는 연도를 빼고 보여준다.
function fmtTime(ts) {
  return ts ? epochToText(ts).slice(5) : '-';
}

//-- 표를 읽기 전 상태로 되돌린다.
//
//   장치를 새로 연결하거나 분리하면 화면의 목록은 더 이상 지금 보드의 것이
//   아니다. 그대로 두면 업데이트 뒤 재연결했을 때 옛 목록을 보고 "기록이 안
//   남았다" 고 오해한다. 보드 플래시의 로그는 건드리지 않는다 - 전원을 뽑아도
//   남는 것이 그 로그의 존재 이유다(지우려면 CLI 의 `boot log clear`).
//
function resetLogTable($) {
  const tb = $('fwLog');
  const cnt = $('fwLogCnt');
  if (tb)  tb.innerHTML = '<tr><td colspan="7" class="muted">읽기를 누른다</td></tr>';
  if (cnt) cnt.textContent = '';
}

// 장치가 빠지면 화면도 비운다(index.html 이 부른다).
export function onDisconnect({ $ }) {
  resetLogTable($);
}

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

  //-- FW_JUMP(0x0009) 는 양쪽에서 "반대편으로 넘어간다" 는 뜻이다.
  //
  //     부트로더 : 앱으로 점프      -> "펌웨어 실행"
  //     앱       : resetToBoot()    -> "부트로더로 재진입"
  //
  //   그래서 커맨드는 하나이고 라벨만 모드에 따라 바뀐다.
  $('fwBoot').onclick = async () => {
    const ch = channel();
    if (!ch) return;
    await ch.request(BOOT_CMD.FW_JUMP, null, 3000).catch(() => {});
    log(bootMode === DEV_MODE_BOOT
          ? '펌웨어를 실행한다. 장치가 다시 열거되면 연결을 누른다.'
          : '부트로더로 재진입한다. 장치가 다시 열거되면 연결을 누른다.', 'muted');
  };
  $('fwLogGet').onclick = () => readLog(ctx).catch(e => log(`로그 읽기 실패: ${e.message}`, 'err'));
}

export async function refresh({ $, channel, isActive }) {
  const ch = channel();
  if (!ch) return;

  const ri = await ch.request(BOOT_CMD.INFO);
  if (ri.err) throw new Error(`INFO err=0x${ri.err.toString(16)}`);
  const info = parseInfo(ri.data);

  const rv = await ch.request(BOOT_CMD.VERSION);
  if (rv.err) throw new Error(`VERSION err=0x${rv.err.toString(16)}`);
  const v = parseVersion(rv.data, info.slotMax);

  if (!isActive(id)) return;          // 기다리는 동안 탭이 바뀌었다

  bootMode = info.mode;
  $('fwBoot').textContent = (info.mode === DEV_MODE_BOOT)
    ? '펌웨어 실행' : '부트로더로 재진입';
  $('fwBoot').hidden = false;
  resetLogTable($);                   // 이전 연결의 목록을 남기지 않는다

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
  const { $, log, channel, isActive } = ctx;
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

    //   응답을 확인해야 한다. 슬롯 내용이 지금 FIRM 과 같으면 부트로더가 할 일이
    //   없어서, 그냥 리셋하면 아무 일도 없이 재부팅만 한다. 그걸 "적용됐다" 고
    //   말하면 안 된다.
    const ru = await ch.request(BOOT_CMD.FW_UPDATE, null, 5000).catch(() => null);

    if (ru && ru.err === ERR_NO_PENDING) {
      log('  적용할 것이 없다. 슬롯에 받아둔 이미지가 지금 실행 중인 것과 같다.', 'muted');
      log('  슬롯 기록은 끝났다. 다른 펌웨어를 올리면 그때 적용된다.', 'muted');
      // 슬롯 내용은 바뀌었으니 표를 새로 읽는다. 안 하면 옛 seq/CRC 가 남아
      // "기록이 안 됐나" 로 보인다.
      await refresh(ctx).catch(() => {});
      return;
    }
    if (ru && ru.err) {
      log(`  적용 요청 실패 err=0x${ru.err.toString(16).padStart(4, '0')}`, 'err');
      return;
    }

    log('  적용 요청 완료. 부트로더가 FIRM 에 복사한 뒤 재부팅한다.', 'ok');
    log('  장치가 분리된다. 다시 열거되면 연결을 누른다.', 'muted');
  } finally {
    $('fwGo').disabled = false;
  }
}

async function readLog({ $, channel, isActive }) {
  const ch = channel();
  if (!ch) return;

  const rc = await ch.request(BOOT_CMD.LOG_COUNT);
  if (rc.err) throw new Error(`LOG_COUNT err=0x${rc.err.toString(16)}`);
  if (rc.data.length < 2) throw new Error('LOG_COUNT 응답이 짧다');

  const n = new DataView(rc.data.buffer, rc.data.byteOffset).getUint16(0, true);
  if (!isActive(id)) return;
  $('fwLogCnt').textContent = `${n} 건`;
  if (n === 0) {
    $('fwLog').innerHTML = '<tr><td colspan="7" class="muted">기록 없음</td></tr>';
    return;
  }
  const rows = [];
  for (let i = 0; i < n; i++) {
    const idx = new Uint8Array(2);
    new DataView(idx.buffer).setUint16(0, i, true);
    const r = await ch.request(BOOT_CMD.LOG_READ, idx);
    if (r.err || r.data.length < 32) continue;
    const g = parseLog(r.data);
    const h = (x) => '0x' + x.toString(16).toUpperCase().padStart(4, '0');
    rows.push(`<tr><td>${i}</td><td>${fmtTime(g.timestamp)}</td><td>${EVT_NAME[g.event] || '?'}</td>
      <td>${g.slot === 0xFF ? '-' : g.slot}</td><td>${h(g.fromCrc)}</td><td>${h(g.toCrc)}</td>
      <td>${g.faultPc ? '0x' + g.faultPc.toString(16).padStart(8, '0') : '-'}</td></tr>`);
  }
  if (!isActive(id)) return;
  $('fwLog').innerHTML = rows.join('');
  // 최신 기록이 아래에 쌓이므로 열자마자 끝으로 보낸다.
  const box = $('fwLog').closest('.tbl-scroll');
  if (box) box.scrollTop = box.scrollHeight;
}
