//-- 보드 정보 패널
//
//   지금은 INFO 가 보고하는 플래시 배치만 보여준다. 부트로더와 앱 모두
//   같은 커맨드로 답하므로 패널은 하나면 된다.
//   네트워크(BOOT_CMD_NET)와 시각(BOOT_CMD_RTC), LAN 스캔(BOOT_CMD_NET_SCAN)이
//   여기에 붙어 있다.
//
import { BOOT_CMD, parseInfo, parseNet, parseScan, DEV_MODE_BOOT, DEV_MODE_APP,
         RTC_OP_GET, RTC_OP_SET, epochToText, localEpoch } from '../boot.js';

export const id    = 'board';
export const title = '보드 정보';

// 부트로더/앱 양쪽에서 볼 수 있다.
export const modes = [DEV_MODE_BOOT, DEV_MODE_APP];

export function render() {
  return `
    <div class="card">
      <table><tbody id="bdTbl">
        <tr><td colspan="2" class="muted">연결 후 표시된다</td></tr>
      </tbody></table>
    </div>

    <div class="card">
      <div class="row"><b>네트워크</b>
        <button id="bdNetGet" class="small">읽기</button>
        <a id="bdNetOpen" class="small" hidden target="_blank" rel="noopener">보드 웹페이지 열기</a></div>
      <table><tbody id="bdNetTbl">
        <tr><td colspan="2" class="muted">연결 후 표시된다</td></tr>
      </tbody></table>
    </div>

    <div class="card">
      <div class="row"><b>LAN 의 보드</b>
        <button id="bdScan" class="small">스캔</button>
        <span class="muted small" id="bdScanCnt"></span></div>
      <div class="tbl-scroll" style="max-height:220px">
        <table><thead><tr>
          <th>IP</th><th>모드</th><th>이름</th><th>버전</th><th>MAC</th><th></th>
        </tr></thead>
        <tbody id="bdScanTbl">
          <tr><td colspan="6" class="muted">스캔을 누른다</td></tr>
        </tbody></table>
      </div>
      <p class="muted small" style="margin:8px 0 0">
        브라우저는 네트워크를 직접 훑을 수 없다. USB 로 붙은 이 보드가 대신
        브로드캐스트를 던지고 응답한 보드를 모아 돌려준다.
      </p>
    </div>

    <div class="card">
      <div class="row"><b>보드 시각</b>
        <button id="bdRtcGet" class="small">읽기</button>
        <button id="bdRtcSet" class="small">PC 시간과 맞추기</button>
        <label class="muted small" style="display:flex;align-items:center;gap:5px">
          <input type="checkbox" id="bdRtcAuto" checked> 1초마다 갱신</label></div>
      <table><tbody>
        <tr><th>보드</th><td id="bdRtcNow" class="muted">-</td></tr>
        <tr><th>PC</th><td id="bdRtcPc" class="muted">-</td></tr>
        <tr><th>차이</th><td id="bdRtcDiff" class="muted">-</td></tr>
      </tbody></table>
      <p class="muted small" style="margin:8px 0 0">
        보드에 코인셀이 없어 전원을 뽑으면 시각이 지워진다. 부트 이벤트 로그의
        시각도 이 RTC 를 쓴다.
      </p>
    </div>
    <div class="card muted small">
      하드웨어 정보와 기능 테스트는 전용 커맨드를 추가한 뒤 여기에 붙인다.
      그 전까지는 CLI 패널에서 확인한다.
    </div>`;
}

//-- 자동 갱신 타이머.
//
//   패널에는 unmount 훅이 없다. 그래서 타이머가 스스로 isActive() 를 확인하고
//   물러난다. mount() 는 탭을 누를 때마다 불리므로 항상 이전 타이머를 먼저 끈다.
//   응답을 기다리는 중이면 건너뛴다 - 겹쳐 보내면 채널이 밀린다.
//
let timer   = null;
let inFlight = false;

function stopAuto() {
  if (timer !== null) { clearInterval(timer); timer = null; }
}

function startAuto(ctx) {
  stopAuto();
  timer = setInterval(async () => {
    if (!ctx.isActive(id) || !ctx.channel()) { stopAuto(); return; }
    if (inFlight || !ctx.$('bdRtcAuto') || !ctx.$('bdRtcAuto').checked) return;

    inFlight = true;
    try { await readRtc(ctx); } catch (e) { /* 한 번 실패는 넘긴다 */ }
    finally { inFlight = false; }
  }, 1000);
}

export function mount(ctx) {
  const { $, log } = ctx;

  $('bdRtcGet').onclick = () => readRtc(ctx).catch(e => log(`시각 읽기 실패: ${e.message}`, 'err'));
  $('bdRtcSet').onclick = () => syncRtc(ctx).catch(e => log(`시각 동기화 실패: ${e.message}`, 'err'));
  $('bdRtcAuto').onchange = (e) => { if (!e.target.checked) stopAuto(); else startAuto(ctx); };
  $('bdNetGet').onclick   = () => readNet(ctx).catch(e => log(`네트워크 읽기 실패: ${e.message}`, 'err'));
  $('bdScan').onclick     = () => scanNet(ctx).catch(e => log(`스캔 실패: ${e.message}`, 'err'));

  startAuto(ctx);
}

//-- 네트워크 상태.
//
//   보드가 IP 를 받으면 "보드 웹페이지 열기" 가 나타난다. 이 페이지는 HTTPS
//   (GitHub Pages)라 http://보드IP 로 fetch/WebSocket 을 걸 수 없다(mixed
//   content). 다만 **최상위 이동은 허용**되므로 새 탭으로 여는 것은 된다.
//   그 뒤로는 보드 자체 웹서버가 상대한다.
//
//-- LAN 스캔.
//
//   보드가 800ms 동안 응답을 모으므로 타임아웃을 넉넉히 준다.
//   자기 자신도 목록에 들어간다 - 지금 USB 로 붙어 있는 보드다.
//
async function scanNet(ctx) {
  const { $, channel, isActive } = ctx;
  const ch = channel();
  if (!ch) return;

  $('bdScan').disabled = true;
  $('bdScanCnt').textContent = '스캔 중...';
  try {
    const r = await ch.request(BOOT_CMD.NET_SCAN, null, 8000);
    if (r.err) throw new Error(`SCAN err=0x${r.err.toString(16)}`);

    const list = parseScan(r.data);
    if (!isActive(id)) return;

    // 지금 USB 로 붙어 있는 보드가 어느 것인지 표시하려고 자기 IP 를 받아둔다.
    const self = parseNet((await ch.request(BOOT_CMD.NET)).data).ip;
    if (!isActive(id)) return;

    $('bdScanCnt').textContent = `${list.length} 대`;
    $('bdScanTbl').innerHTML = list.length === 0
      ? '<tr><td colspan="6" class="muted">응답한 보드가 없다</td></tr>'
      : list.map(b => `<tr>
          <td>${b.ip}</td>
          <td>${b.mode === DEV_MODE_BOOT ? '부트로더' : '앱'}</td>
          <td>${b.name}</td><td>${b.version}</td><td>${b.mac}</td>
          <td>${b.ip === self
                ? '<span class="muted">이 보드</span>'
                : `<a href="http://${b.ip}/" target="_blank" rel="noopener">열기</a>`}</td>
        </tr>`).join('');
  } finally {
    $('bdScan').disabled = false;
  }
}

async function readNet(ctx) {
  const { $, channel, isActive } = ctx;
  const ch = channel();
  if (!ch) return;

  const r = await ch.request(BOOT_CMD.NET);
  if (r.err) throw new Error(`NET err=0x${r.err.toString(16)}`);
  const n = parseNet(r.data);
  if (!isActive(id)) return;

  const link = $('bdNetOpen');

  if (!n.valid) {
    $('bdNetTbl').innerHTML =
      '<tr><td colspan="2" class="muted">이 쪽에는 이더넷이 없다 (부트로더)</td></tr>';
    link.hidden = true;
    return;
  }

  const yn = (v, ok, ng) => `<span class="${v ? 'ok' : 'err'}">${v ? ok : ng}</span>`;

  //   DHCP 로 받기 전에는 펌웨어에 박아둔 기본값이 그대로 보인다. 그걸 보드
  //   주소인 것처럼 보여주면 안 된다. 할당 전임을 붙여서 알린다.
  const assigned = n.ipGet;
  const note = assigned ? '' : ' <span class="muted">(기본값, 할당 전)</span>';

  $('bdNetTbl').innerHTML = `
    <tr><th>링크</th><td>${yn(n.link, '연결됨', '케이블 연결 안 됨')}</td></tr>
    <tr><th>주소</th><td>${n.dhcp ? 'DHCP' : '고정 IP'} ·
        ${yn(assigned, '할당됨', '아직 못 받음')}</td></tr>
    <tr><th>IP</th><td>${n.ip}${note}</td></tr>
    <tr><th>서브넷</th><td>${n.sn}</td></tr>
    <tr><th>게이트웨이</th><td>${n.gw}</td></tr>
    <tr><th>DNS</th><td>${n.dns}</td></tr>
    <tr><th>MAC</th><td>${n.mac}</td></tr>`;

  if (n.ipGet && n.ip !== '0.0.0.0') {
    link.href   = `http://${n.ip}/`;
    link.textContent = `보드 웹페이지 열기 (${n.ip})`;
    link.hidden = false;
  } else {
    link.hidden = true;
  }
}

//-- 보드 시각을 읽어 PC 와 비교한다.
//
//   GET 이든 SET 이든 펌웨어는 항상 현재 값을 돌려준다. 그래서 맞춘 직후에도
//   같은 코드로 결과를 확인할 수 있다.
//
async function showRtc(ctx, r) {
  const { $, isActive } = ctx;
  if (r.err) throw new Error(`RTC err=0x${r.err.toString(16)}`);
  if (r.data.length < 4) throw new Error(`RTC 응답이 짧다 (${r.data.length}B)`);
  if (!isActive(id)) return;

  const dv    = new DataView(r.data.buffer, r.data.byteOffset);
  const epoch = dv.getUint32(0, true);
  const pc    = localEpoch();

  $('bdRtcNow').textContent = epochToText(epoch);
  $('bdRtcPc').textContent  = epochToText(pc);

  if (!epoch) {
    $('bdRtcNow').className  = 'muted';
    $('bdRtcDiff').textContent = '보드 시각이 설정되지 않았다';
    $('bdRtcDiff').className   = 'muted';
    return;
  }

  const diff = pc - epoch;
  const abs  = Math.abs(diff);

  $('bdRtcNow').className = '';
  if (abs <= 2) {
    $('bdRtcDiff').textContent = 'PC 시간과 일치한다';
    $('bdRtcDiff').className   = 'ok';
  } else {
    const unit = abs < 60    ? `${abs}초`
               : abs < 3600  ? `${Math.round(abs / 60)}분`
               : abs < 86400 ? `${Math.round(abs / 3600)}시간`
               :               `${Math.round(abs / 86400)}일`;
    $('bdRtcDiff').textContent = `PC 시간보다 ${unit} ${diff > 0 ? '느리다' : '빠르다'}`;
    $('bdRtcDiff').className   = 'err';
  }
}

async function readRtc(ctx) {
  const ch = ctx.channel();
  if (!ch) return;
  await showRtc(ctx, await ch.request(BOOT_CMD.RTC, new Uint8Array([RTC_OP_GET])));
}

async function syncRtc(ctx) {
  const ch = ctx.channel();
  if (!ch) return;

  const req = new Uint8Array(5);
  req[0] = RTC_OP_SET;
  new DataView(req.buffer).setUint32(1, localEpoch(), true);

  await showRtc(ctx, await ch.request(BOOT_CMD.RTC, req));
  ctx.log('보드 시각을 PC 와 맞췄다.', 'ok');
}

export async function refresh(ctx) {
  const { $, channel, isActive } = ctx;
  const ch = channel();
  if (!ch) return;

  const r = await ch.request(BOOT_CMD.INFO);
  if (r.err) throw new Error(`INFO err=0x${r.err.toString(16)}`);
  const i = parseInfo(r.data);

  if (!isActive(id)) return;      // 기다리는 동안 탭이 바뀌었다
  const hx = (v) => '0x' + v.toString(16).toUpperCase().padStart(8, '0');
  const kb = (v) => `${(v / 1024).toFixed(0)} KB`;

  const common = `
    <tr><th>실행 중</th><td>${i.mode === DEV_MODE_BOOT ? '부트로더' : '앱'}
        · ${i.name} ${i.version}</td></tr>`;

  const flash = `
    <tr><th>BOOT</th><td>${hx(i.bootAddr)}</td></tr>
    <tr><th>FIRM</th><td>${hx(i.firmAddr)} · ${kb(i.firmSize)}</td></tr>
    <tr><th>슬롯</th><td>${i.slotMax} 개 · ${kb(i.slotSize)}</td></tr>
    <tr><th>UF2 familyID</th><td>${hx(i.familyId)}</td></tr>`;

  // 앱 모드에서는 네트워크/하드웨어 정보가 여기에 더 붙는다.
  $('bdTbl').innerHTML = common + flash;

  await readNet(ctx);
  await readRtc(ctx);

  // 연결이 끊겼다 다시 붙으면 mount() 는 다시 불리지 않는다(탭이 그대로라서).
  // 갱신은 여기서도 되살린다. startAuto() 는 항상 이전 타이머를 먼저 끈다.
  startAuto(ctx);
}
