//-- 보드 정보 패널
//
//   지금은 부트로더가 보고하는 플래시 배치만 보여준다.
//   앱(stm32h5-fw)에 HID 채널이 붙으면 IP/MAC/하드웨어 정보와 테스트 항목을
//   여기에 추가한다. 프로토콜 계층(proto.js)은 그대로 쓰면 된다.
//
import { BOOT_CMD, parseInfo, DEV_MODE_BOOT, DEV_MODE_APP } from '../boot.js';

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
    <div class="card muted small">
      앱에 HID 채널이 붙으면 네트워크(IP/MAC/DHCP)와 하드웨어 정보,
      기능 테스트를 이 패널에 추가한다.
    </div>`;
}

export function mount() {}

export async function refresh({ $, channel }) {
  const ch = channel();
  if (!ch) return;

  const i = parseInfo((await ch.request(BOOT_CMD.INFO)).data);
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

  // 앱 모드에서는 네트워크/하드웨어 정보가 여기에 더 붙는다(앱에 HID 가 붙은 뒤).
  $('bdTbl').innerHTML = common + flash;
}
