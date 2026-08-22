//-- CLI 터미널 패널
//
//   보드의 CLI 를 그대로 웹에서 친다. 펌웨어 쪽은 cmd 패킷 위에 얹은 가상 UART
//   채널(cli_cmd.c)이라 cli.c 는 아무것도 모른 채 동작한다.
//
//   전송이 HID 든 (향후) W6300 네트워크든 동일하다. proto.js 의 Channel 만 갈아끼우면 된다.
//
import { BOOT_CMD, DEV_MODE_BOOT, DEV_MODE_APP } from '../boot.js';

export const id    = 'cli';
export const title = 'CLI';

// 부트로더/앱 양쪽에서 쓸 수 있다.
export const modes = [DEV_MODE_BOOT, DEV_MODE_APP];

const HISTORY_MAX = 50;
let history = [];
let histIdx = -1;

export function render() {
  return `
    <div class="card">
      <pre id="cliTerm" class="term">보드의 CLI 를 여기서 친다. 'help' 로 시작한다.\n</pre>
      <div class="row" style="margin-top:10px">
        <input id="cliIn" type="text" placeholder="명령 입력 후 Enter"
               autocomplete="off" spellcheck="false" style="flex:1;min-width:220px">
        <button id="cliSend" class="primary">보내기</button>
        <button id="cliClear" class="small">지우기</button>
      </div>
      <p class="muted small" style="margin:8px 0 0">
        위/아래 화살표로 이전 명령을 불러온다.
      </p>
    </div>`;
}

export function mount(ctx) {
  const { $, log, channel } = ctx;

  const put = (text, cls) => {
    const t = $('cliTerm');
    const span = cls ? `<span class="${cls}">${esc(text)}</span>` : esc(text);
    t.innerHTML += span;
    t.scrollTop = t.scrollHeight;
  };

  const send = async () => {
    const line = $('cliIn').value.trim();
    if (!line) return;
    const ch = channel();
    if (!ch) { log('장치가 연결되지 않았다.', 'err'); return; }

    $('cliIn').value = '';
    history = [line, ...history.filter(h => h !== line)].slice(0, HISTORY_MAX);
    histIdx = -1;

    try {
      const r = await ch.request(BOOT_CMD.CLI, new TextEncoder().encode(line), 6000);
      if (r.err) { put(`\n[err 0x${r.err.toString(16).padStart(4, '0')}]\n`, 'err'); return; }
      put('\n' + new TextDecoder().decode(r.data));
    } catch (e) {
      put(`\n[${e.message}]\n`, 'err');
    }
  };

  $('cliSend').onclick = send;
  $('cliClear').onclick = () => { $('cliTerm').textContent = ''; };

  $('cliIn').onkeydown = (e) => {
    if (e.key === 'Enter') { e.preventDefault(); send(); return; }
    if (e.key === 'ArrowUp') {
      e.preventDefault();
      if (histIdx + 1 < history.length) $('cliIn').value = history[++histIdx];
    }
    if (e.key === 'ArrowDown') {
      e.preventDefault();
      if (histIdx > 0)       $('cliIn').value = history[--histIdx];
      else { histIdx = -1; $('cliIn').value = ''; }
    }
  };
}

export async function refresh({ $ }) {
  $('cliIn').focus();
}

const esc = (s) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
