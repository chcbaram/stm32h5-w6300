#!/usr/bin/env python3
"""웹페이지를 gzip 해서 펌웨어에 넣을 C 배열로 만든다.

    python3 gen_web.py <repo_root> <출력 .c>

번들러를 쓰지 않는다. 각 파일을 따로 gzip 해서 표에 담고, 서버가 경로로 찾아
그대로 내보낸다. ES 모듈을 한 파일로 합치려면 import 를 풀고 이름 충돌을
정리해야 하는데(패널마다 id/title/render 가 같은 이름이다) 그 위험을 감수할
이유가 없다. 파일이 여섯 개뿐이라 표가 더 싸고 확실하다.
"""
import gzip
import os
import re
import sys

#-- 모듈을 하나로 묶는다.
#
#   보드는 소켓을 셋 밖에 못 듣는데(0~4 는 telnet/DHCP/SNTP/cmd TCP/discovery 가
#   쓴다) 브라우저는 파일마다 연결을 연다. 파일이 여섯이면 셋은 연결조차 못 하고
#   페이지가 반쯤 뜬다. 실제로 동시 요청 여섯 개 중 셋이 실패했다.
#
#   그래서 JS 다섯 개를 app.js 하나로 묶어 요청을 둘로 줄인다. index.html 은
#   그대로 두고 import 경로만 바꾼다.
#
#   묶는 방식은 작은 레지스트리다. 각 모듈을 함수로 감싸므로 스코프가 그대로
#   유지된다 - 파일을 이어붙이는 방식이면 패널마다 있는 id/title/render 가
#   서로 덮어써서 못 쓴다.
#
MODULES = [
    "web/proto.js",
    "web/boot.js",
    "web/panels/firmware.js",
    "web/panels/board.js",
    "web/panels/cli.js",
]

FILES = [
    ("/",           None,        "text/html"),        # index.html (경로 치환)
    ("/web/app.js", None,        "text/javascript"),  # 묶은 모듈
]


#   import 는 여러 줄에 걸칠 수 있다. 줄 단위 정규식이면 놓친다.
#   실제로 firmware.js 의 두 줄짜리 import 가 그대로 남아 번들이 깨졌다.
MOD_RE_FROM  = re.compile(r"import\s+([\s\S]+?)\s+from\s+['\"]([^'\"]+)['\"];?")
EXPORT_RE    = re.compile(r"^(\s*)export\s+(?=(?:const|let|var|function|class|async)\b)", re.M)
EXPORT_NAME  = re.compile(
    r"^\s*export\s+(?:async\s+)?(?:const|let|var|function|class)\s+([A-Za-z_$][\w$]*)", re.M)


def mod_key(path):
    """모듈을 가리키는 이름. 경로가 어떻게 적혔든 파일 이름으로 통일한다."""
    return os.path.basename(path)


def wrap_module(path, src):
    """ES 모듈을 레지스트리용 함수로 감싼다."""
    names = EXPORT_NAME.findall(src)

    def repl_import(m):
        what, frm = m.group(1), m.group(2)
        key = mod_key(frm)
        if what.startswith("*"):                      # import * as ns from '...'
            ns = what.split(" as ")[1].strip()
            return f"const {ns} = __req('{key}');"
        what = what.strip()
        if what.startswith("{"):                      # import { a, b as c } from '...'
            inner = " ".join(what.strip("{} \n").split())   # 줄바꿈을 한 줄로
            inner = inner.replace(" as ", ": ")
            return f"const {{{inner}}} = __req('{key}');"
        return f"const {what} = __req('{key}').default;"

    body = MOD_RE_FROM.sub(repl_import, src)
    body = EXPORT_RE.sub(r"\1", body)

    assigns = "\n".join(f"  __x.{n} = {n};" for n in names)
    return (f"__def('{mod_key(path)}', function (__x, __req) {{\n"
            f"{body}\n{assigns}\n}});\n")


def build_bundle(root):
    parts = ["""// 자동 생성. tools/web/gen_web.py 가 묶는다.
//
//   각 모듈을 함수로 감싸 스코프를 지킨다. 그래야 패널마다 있는 id/title/render
//   같은 같은 이름들이 서로 덮어쓰지 않는다.
//
const __mods = {};
function __def(name, fn) { __mods[name] = { fn, ex: null }; }
function __req(name) {
  const m = __mods[name];
  if (!m) throw new Error('모듈 없음: ' + name);
  if (!m.ex) { m.ex = {}; m.fn(m.ex, __req); }
  return m.ex;
}
"""]
    for rel in MODULES:
        parts.append(wrap_module(rel, open(os.path.join(root, rel)).read()))

    # 바깥에서 쓸 이름만 다시 내보낸다.
    parts.append("""
export const { HidChannel, HttpChannel, isBoardHosted } = __req('proto.js');
export const { BOOT_CMD, parseInfo, DEV_MODE_BOOT } = __req('boot.js');
export const firmware = __req('firmware.js');
export const board    = __req('board.js');
export const cli      = __req('cli.js');
""")
    return "".join(parts)


def build_index(root):
    """index.html 의 import 를 묶은 파일 하나로 바꾼다."""
    src = open(os.path.join(root, "index.html")).read()
    new_import = ("import { HidChannel, HttpChannel, isBoardHosted,\n"
                  "         BOOT_CMD, parseInfo, DEV_MODE_BOOT,\n"
                  "         firmware, board, cli } from './web/app.js';")
    src = MOD_RE_FROM.sub(lambda m: "", src, count=0)
    return src.replace("<script type=\"module\">", "<script type=\"module\">\n" + new_import, 1)


def c_array(name, data):
    out = [f"static const uint8_t {name}[] = {{"]
    for i in range(0, len(data), 16):
        row = ", ".join(f"0x{b:02X}" for b in data[i:i + 16])
        out.append(f"  {row},")
    out.append("};")
    return "\n".join(out)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)

    root, out_path = sys.argv[1], sys.argv[2]
    parts, table, total_raw, total_gz = [], [], 0, 0

    contents = {
        "/":           build_index(root).encode(),
        "/web/app.js": build_bundle(root).encode(),
    }

    for i, (url, rel, mime) in enumerate(FILES):
        raw = contents[url]
        gz = gzip.compress(raw, 9, mtime=0)      # mtime=0 : 빌드마다 결과가 같아야 한다

        parts.append(c_array(f"web_data_{i}", gz))
        table.append(f'  {{ "{url}", "{mime}", web_data_{i}, sizeof(web_data_{i}) }},')
        total_raw += len(raw)
        total_gz  += len(gz)
        print(f"  {url:<26} {len(raw):>6} -> {len(gz):>6} B")

    body = "\n\n".join(parts)
    tbl  = "\n".join(table)

    with open(out_path, "w") as f:
        f.write(f'''// 자동 생성. 고치지 말 것. tools/web/gen_web.py 가 만든다.
//
//   원본 {total_raw} B -> gzip {total_gz} B
//
//   각 파일을 따로 gzip 해서 담는다. 서버는 경로로 찾아 그대로 내보내고
//   Content-Encoding: gzip 을 붙인다. 브라우저가 알아서 푼다.
//
#include "net_http.h"

#ifdef _USE_HW_WIZNET

{body}

const web_file_t web_file_tbl[] = {{
{tbl}
}};

const uint32_t web_file_cnt = sizeof(web_file_tbl) / sizeof(web_file_tbl[0]);

#endif
''')

    print(f"  {'합계':<26} {total_raw:>6} -> {total_gz:>6} B  ({out_path})")


if __name__ == "__main__":
    main()
