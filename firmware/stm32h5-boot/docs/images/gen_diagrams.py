#!/usr/bin/env python3
"""문서용 그림 생성기 (메모리 맵은 gen_memory_map.py 에 따로 있다).

  python3 gen_diagrams.py

  slot-pingpong.svg   v1 -> v2 -> 롤백 -> v3 상태 전이
  rollback-flow.svg   부팅 판정과 롤백 흐름
  fault-recovery.svg  폴트 카운터와 자동 복구
  ota-paths.svg       업데이트 경로와 적용 순서

확인:  rsvg-convert -b '#ffffff' -o /tmp/a.png slot-pingpong.svg
       rsvg-convert -b '#0d1117' -o /tmp/b.png slot-pingpong.svg

주의(gen_memory_map.py 와 같은 교훈):
  - 글자는 반드시 블록 '안'에 둔다. 바깥에 두면 옆 칸과 겹친다.
  - 색은 prefers-color-scheme 에 의존하지 않는다. rsvg 는 미디어쿼리를 무시한다.
    밝은 배경과 어두운 배경 양쪽에서 읽히는 값을 쓴다.
  - 글자 폭은 monospace 기준 대략 0.6 * font-size 로 잡고 칸 너비를 정한다.
"""

FONT = "ui-monospace, SFMono-Regular, Menlo, monospace"

C_FIRM  = "#2a9d8f"      # 실행 영역
C_KEEP  = "#3d5a80"      # 롤백 대상(보존)
C_OLD   = "#8d99ae"      # 낡은 이미지(다음 수신 대상)
C_DEAD  = "#b0413e"      # 무효
C_LINE  = "#7a869a"
C_TEXT  = "#e8eaed"      # 블록 안 글자
C_LABEL = "#7a869a"      # 블록 밖 글자 - 양쪽 테마에서 읽히는 중간 회색


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def box(x, y, w, h, fill, lines, fs=12):
    out = [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="4" fill="{fill}"/>']
    n = len(lines)
    y0 = y + h / 2 - (n - 1) * (fs + 3) / 2 + fs / 3
    for i, t in enumerate(lines):
        out.append(
            f'<text x="{x + w/2}" y="{y0 + i*(fs+3):.0f}" fill="{C_TEXT}" '
            f'font-family="{FONT}" font-size="{fs}" text-anchor="middle">{esc(t)}</text>')
    return "".join(out)


def label(x, y, t, fs=12, anchor="middle", weight="normal", color=C_LABEL):
    return (f'<text x="{x}" y="{y}" fill="{color}" font-family="{FONT}" '
            f'font-size="{fs}" font-weight="{weight}" text-anchor="{anchor}">{esc(t)}</text>')


def arrow(x1, y1, x2, y2):
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{C_LINE}" '
            f'stroke-width="1.5" marker-end="url(#a)"/>')


DEFS = (f'<defs><marker id="a" viewBox="0 0 10 10" refX="9" refY="5" '
        f'markerWidth="6" markerHeight="6" orient="auto">'
        f'<path d="M0,0 L10,5 L0,10 z" fill="{C_LINE}"/></marker></defs>')


# ---------------------------------------------------------------- 상태 전이
def gen_pingpong():
    #   한 단계 = FIRM / SLOT0 / SLOT1 세 칸. 칸 안에 버전과 seq 를 같이 적는다.
    #   아래에는 그 상태에서 세 함수가 무엇을 돌려주는지 적는다.
    STEPS = [
        ("① v1 부팅 중",        [("v1", 1, C_FIRM), ("v1", 1, C_KEEP), ("-", 0, C_DEAD)],
         ["write    1", "pending -1", "rollback -1"]),
        ("② v2 받아 적용",      [("v2", 2, C_FIRM), ("v1", 1, C_KEEP), ("v2", 2, C_KEEP)],
         ["write    0", "pending -1", "rollback 0"]),
        ("③ v2 부팅 실패\n→ 롤백", [("v1", 1, C_FIRM), ("v1", 1, C_KEEP), ("-", 0, C_DEAD)],
         ["write    1", "pending -1", "rollback -1"]),
        ("④ v3 받아 적용",      [("v3", 3, C_FIRM), ("v1", 1, C_KEEP), ("v3", 3, C_KEEP)],
         ["write    0", "pending -1", "rollback 0"]),
    ]
    ROWS = ["FIRM", "SLOT0", "SLOT1"]

    BW, BH, GAP = 150, 46, 14
    LEFT, TOP   = 74, 78
    COLGAP      = 52
    W = LEFT + len(STEPS) * BW + (len(STEPS) - 1) * COLGAP + 20
    H = TOP + len(ROWS) * (BH + GAP) + 92

    s = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" '
         f'viewBox="0 0 {W} {H}">', DEFS]
    s.append(label(LEFT, 26, "슬롯 핑퐁 — 두 슬롯을 번갈아 쓴다", 14, "start", "bold"))
    s.append(label(LEFT, 44, "FIRM 과 같은 슬롯 = 롤백 대상(보존)   ·   다른 슬롯 = 다음 수신 대상",
                   11, "start"))

    for r, name in enumerate(ROWS):
        y = TOP + r * (BH + GAP)
        s.append(label(LEFT - 12, y + BH / 2 + 4, name, 12, "end"))

    for c, (title, cells, foot) in enumerate(STEPS):
        x = LEFT + c * (BW + COLGAP)
        for i, ln in enumerate(title.split("\n")):
            s.append(label(x + BW / 2, 62 + i * 14, ln, 12, "middle", "bold"))
        for r, (ver, seq, col) in enumerate(cells):
            y = TOP + r * (BH + GAP)
            txt = ["(비어 있음)"] if ver == "-" else [ver, f"seq {seq}"]
            s.append(box(x, y, BW, BH, col, txt))
        # 한 줄로 쓰면 옆 칸과 겹친다. 칸 폭이 150px 인데 글자가 그보다 길다.
        for i, ln in enumerate(foot):
            s.append(label(x + BW / 2, TOP + len(ROWS) * (BH + GAP) + 14 + i * 13, ln, 10))

        if c < len(STEPS) - 1:
            ay = TOP + (BH + GAP) * 1 + BH / 2
            s.append(arrow(x + BW + 8, ay, x + BW + COLGAP - 8, ay))

    s.append(label(LEFT, H - 12,
                   "③ 에서 실패한 이미지가 든 슬롯은 무효화한다. 그래야 같은 것을 다시 적용하지 않는다.",
                   10, "start"))
    s.append("</svg>")
    return "".join(s)


# ---------------------------------------------------------------- 롤백 흐름
def gen_rollback():
    #   위에서 아래로 한 줄. 분기는 오른쪽으로 뺀다.
    BW, BH = 300, 44
    X, W   = 40, 830
    steps = [
        ("부팅. 부트로더가 판정한다",                       C_OLD,  None),
        ("fault_cnt >= 3 ?",                               C_KEEP, "폴트 리셋이 연속 3회"),
        ("boot_try >= 3 ?",                                C_KEEP, "앱이 10초를 못 버팀"),
        ("bootGetRollbackSlot()",                          C_OLD,  "FIRM 보다 seq 가 작은 유효 슬롯"),
        ("bootApplySlot() → FIRM",                         C_FIRM, "되돌아간다"),
        ("실패한 슬롯 무효화",                              C_DEAD, "같은 것을 다시 적용하지 않게"),
        ("bootJumpFirm()",                                 C_FIRM, None),
    ]
    H = 96 + len(steps) * (BH + 26) + 40

    s = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" '
         f'viewBox="0 0 {W} {H}">', DEFS]
    s.append(label(X, 26, "롤백 — 새 펌웨어가 부팅에 실패하면 되돌린다", 14, "start", "bold"))
    s.append(label(X, 44, "CRC 는 '플래시가 온전한가' 만 본다. '실제로 도는가' 는 아래 두 카운터가 본다.",
                   11, "start"))

    for i, (txt, col, note) in enumerate(steps):
        y = 68 + i * (BH + 26)
        s.append(box(X, y, BW, BH, col, [txt]))
        if note:
            s.append(label(X + BW + 14, y + BH / 2 + 4, note, 11, "start"))
        if i < len(steps) - 1:
            s.append(arrow(X + BW / 2, y + BH + 4, X + BW / 2, y + BH + 22))

    # 되돌아갈 곳이 없을 때. 옆 주석이 끝나는 자리보다 뒤에 둔다.
    y2 = 68 + 3 * (BH + 26)
    s.append(arrow(X + BW + 250, y2 + BH / 2, X + BW + 278, y2 + BH / 2))
    s.append(box(X + BW + 284, y2, 170, BH, C_DEAD, ["없으면 UF2 모드"]))
    s.append(label(X, H - 16,
                   "되돌아갈 곳이 없으면(-1) 무한 롤백 대신 UF2 모드로 빠진다. 사용자가 직접 넣게 한다.",
                   10, "start"))
    s.append("</svg>")
    return "".join(s)


# ------------------------------------------------------- 폴트 자동 복구
def gen_fault():
    """카운터가 왜 필요한지, 어디서 0 이 되는지를 한눈에 보이게 한다."""
    #   되돌림 화살표와 그 라벨이 박스 왼쪽으로 나가므로 여백을 넉넉히 준다.
    #   처음에 X=40 으로 뒀다가 라벨이 캔버스 밖으로 잘렸다.
    BW, BH = 420, 46
    TX     = 40                 # 제목 왼쪽 끝
    X, W   = 150, 1000
    steps = [
        ("① 앱 실행",
         C_FIRM, "하드폴트 (없는 주소 접근 등)"),
        ("② faultReset()",
         C_DEAD, ".noinit 에 R0~PSR · RTC 백업에 fault_cnt++ · 리셋"),
        ("③ 부트로더 : fault_cnt 확인",
         C_KEEP, "백업 레지스터라 리셋에도 살아남는다"),
        ("④ 롤백 + FAULT_RECOVER 기록 + 카운터 0",
         C_OLD,  "fault_cnt >= 3 일 때만"),
        ("⑤ 앱이 10초 버팀 → resetConfirmBoot()",
         C_FIRM, "fault_cnt = 0 · boot_try = 0"),
    ]
    H = 108 + len(steps) * (BH + 30) + 46

    s = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" '
         f'viewBox="0 0 {W} {H}">', DEFS]
    s.append(label(TX, 26, "폴트 자동 복구 — 반복해서 죽는 펌웨어를 되돌린다", 14, "start", "bold"))
    s.append(label(TX, 44, "한 번의 폴트로는 되돌리지 않는다. 연속 3회여야 한다.", 11, "start"))
    s.append(label(TX, 62, "확정에 성공하면 카운터가 0 이 되므로, 오래 잘 돌다 나는 드문 폴트로는 롤백되지 않는다.",
                   11, "start"))

    for i, (txt, col, note) in enumerate(steps):
        y = 88 + i * (BH + 30)
        s.append(box(X, y, BW, BH, col, [txt]))
        if note:
            s.append(label(X + BW + 16, y + BH / 2 + 4, note, 11, "start"))
        if i < len(steps) - 1:
            s.append(arrow(X + BW / 2, y + BH + 4, X + BW / 2, y + BH + 26))

    # ③ 에서 3회 미만이면 그냥 점프하고 ① 로 돌아간다
    y3 = 88 + 2 * (BH + 30)
    y1 = 88
    lx = X - 22
    s.append(f'<path d="M{X} {y3 + BH/2} H{lx} V{y1 + BH/2} H{X - 6}" '
             f'fill="none" stroke="{C_LINE}" stroke-width="1.5" '
             f'stroke-dasharray="4 3" marker-end="url(#a)"/>')
    s.append(label(lx - 6, (y1 + y3) / 2 + BH / 2, "3회 미만이면", 10, "end"))
    s.append(label(lx - 6, (y1 + y3) / 2 + BH / 2 + 13, "그냥 점프", 10, "end"))

    s.append(label(TX, H - 16,
                   "카운터는 RTC 백업 레지스터에 둔다. .noinit 은 SRAM 이라 전원을 뽑으면 사라진다.",
                   10, "start"))
    s.append("</svg>")
    return "".join(s)


# ------------------------------------------------- 업데이트 경로 / OTA
def gen_ota():
    """왜 전송을 하나 더 붙이는 것이 쌌는지, 그리고 왜 두 단계로 나뉘는지."""
    W = 1020

    #-- 위쪽 : 전송계층이 달라도 커맨드 셋은 하나다
    HOSTS = [
        ("download.py --tcp",  "drv_tcp.c",  "TCP 5301"),
        ("download.py",        "drv_usb.c",  "USB CDC"),
        ("웹페이지 (USB)",      "drv_hid.c",  "USB HID"),
        ("웹페이지 (보드)",     "net_http.c", "POST /cmd"),
    ]
    BW1, BH1, VG = 190, 40, 12
    XH, XD, XC   = 40, 262, 520
    TOP          = 92
    n            = len(HOSTS)
    midy         = TOP + (n * (BH1 + VG) - VG) / 2

    s = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="640" '
         f'viewBox="0 0 {W} 640">', DEFS]
    s.append(label(XH, 26, "업데이트 경로 — 전송만 다르고 나머지는 하나다", 14, "start", "bold"))
    s.append(label(XH, 44, "cmd.c 가 전송계층과 무관해서, 새 경로는 여섯 함수(open/close/available/flush/read/write)만 채우면 된다.",
                   11, "start"))
    s.append(label(XH, 62, "이더넷 OTA 가 drv_tcp.c 하나로 끝난 이유다. 부트로더는 손댄 것이 없다.",
                   11, "start"))

    s.append(label(XH + BW1/2, TOP - 14, "호스트", 11, "middle"))
    s.append(label(XD + BW1/2, TOP - 14, "채널 드라이버", 11, "middle"))
    s.append(label(XC + 105,   TOP - 14, "공용", 11, "middle"))

    for i, (host, drv, via) in enumerate(HOSTS):
        y = TOP + i * (BH1 + VG)
        s.append(box(XH, y, BW1, BH1, C_OLD,  [host], 11))
        s.append(box(XD, y, BW1, BH1, C_KEEP, [drv, via], 10))
        s.append(arrow(XH + BW1 + 4, y + BH1/2, XD - 6, y + BH1/2))
        s.append(f'<path d="M{XD + BW1 + 4} {y + BH1/2} H{XC - 30} V{midy}" '
                 f'fill="none" stroke="{C_LINE}" stroke-width="1.5"/>')
    s.append(arrow(XC - 30, midy, XC - 6, midy))

    s.append(box(XC, midy - 46, 210, 40, C_FIRM, ["cmd.c"], 12))
    s.append(box(XC, midy -  2, 210, 40, C_FIRM, ["cmd_boot.c"], 12))
    s.append(label(XC + 105, midy + 60, "커맨드 셋 하나", 11))
    s.append(label(XC + 105, midy + 76, "FW_BEGIN / ERASE / WRITE", 10))
    s.append(label(XC + 105, midy + 90, "END / VERIFY / UPDATE", 10))

    s.append(arrow(XC + 210 + 4, midy - 26, XC + 250, midy - 26))
    s.append(box(XC + 256, midy - 46, 150, 84, C_DEAD, ["슬롯", "(뱅크2)"], 12))

    #-- 아래쪽 : 적용은 두 단계다
    Y2 = 380
    s.append(label(XH, Y2, "적용은 두 단계 — 앱은 자기가 실행 중인 뱅크1 을 지울 수 없다",
                   14, "start", "bold"))
    s.append(label(XH, Y2 + 18,
                   "그래서 앱은 슬롯에 받아두기만 하고, 실제 교체는 다음 부팅에 부트로더가 한다.",
                   11, "start"))

    SEQ = [
        ("① 앱\n슬롯에 기록 + 태그",        C_FIRM),
        ("② resetToUpdate()\n리셋",          C_KEEP),
        ("③ 부트로더\nbootApplySlot()",      C_OLD),
        ("④ 새 앱 실행\n10초 뒤 확정",        C_FIRM),
    ]
    BW2, GAP2 = 200, 48
    ys = Y2 + 48
    for i, (txt, col) in enumerate(SEQ):
        x = XH + i * (BW2 + GAP2)
        s.append(box(x, ys, BW2, 58, col, txt.split("\n"), 12))
        if i < len(SEQ) - 1:
            s.append(arrow(x + BW2 + 6, ys + 29, x + BW2 + GAP2 - 6, ys + 29))

    s.append(label(XH, ys + 92,
                   "부트로더 모드에서는 ①~③ 이 한 번에 끝난다. 지울 뱅크를 실행 중이 아니기 때문이다.",
                   11, "start"))
    s.append(label(XH, ys + 110,
                   "④ 에서 10초를 못 버티면 boot_try 가 쌓이고, 3회면 되돌린다(08 문서).",
                   11, "start"))
    s.append(label(XH, ys + 136,
                   "실측  USB CDC 140 KB/s   ·   이더넷 TCP 90 KB/s   ·   USB HID 35 KB/s",
                   11, "start", "bold"))
    s.append("</svg>")
    return "".join(s)


if __name__ == "__main__":
    import os
    here = os.path.dirname(os.path.abspath(__file__))
    for name, gen in [("slot-pingpong.svg", gen_pingpong),
                      ("rollback-flow.svg", gen_rollback),
                      ("fault-recovery.svg", gen_fault),
                      ("ota-paths.svg", gen_ota)]:
        p = os.path.join(here, name)
        open(p, "w").write(gen())
        print(f"  {name}  {os.path.getsize(p)} B")
