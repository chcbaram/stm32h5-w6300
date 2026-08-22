#!/usr/bin/env python3
"""슬롯 핑퐁과 롤백 그림 생성기.

  python3 gen_slot_diagram.py

  slot-pingpong.svg   v1 -> v2 -> 롤백 -> v3 상태 전이
  rollback-flow.svg   부팅 판정과 롤백 흐름

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


if __name__ == "__main__":
    import os
    here = os.path.dirname(os.path.abspath(__file__))
    for name, gen in [("slot-pingpong.svg", gen_pingpong),
                      ("rollback-flow.svg", gen_rollback),
                      ("fault-recovery.svg", gen_fault)]:
        p = os.path.join(here, name)
        open(p, "w").write(gen())
        print(f"  {name}  {os.path.getsize(p)} B")
