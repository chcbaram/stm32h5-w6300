#!/usr/bin/env python3
"""memory-map.svg 생성기.

  python3 gen_memory_map.py

hw_def.h 의 배치가 바뀌면 아래 BANKS 를 고치고 다시 돌린다.
확인:  rsvg-convert -b '#ffffff' -o /tmp/a.png memory-map.svg
       rsvg-convert -b '#0d1117' -o /tmp/b.png memory-map.svg

주의: 주소/섹터 라벨은 반드시 블록 '안'에 둔다. 바깥에 두면 옆 컬럼과 겹친다.
      색은 prefers-color-scheme 에 의존하지 않고 양쪽에서 읽히는 중간 회색을 쓴다
      (rsvg 등 일부 렌더러는 미디어쿼리를 무시한다).
"""

BANKS = [
    ("Bank 1  0x08000000~0x080FFFFF  app executes here", 0x08100000, [
        ("BOOT",     0x08000000, 128, "B1 0-15",   "#3d5a80", "VECTOR 1K + VER 1K + code 126K"),
        ("FIRM",     0x08020000, 448, "B1 16-71",  "#2a9d8f", "TAG 1K + VEC 1K + VER 1K + code"),
        ("reserved", 0x08090000, 448, "B1 72-127", "#8d99ae", "future littlefs / read-mostly"),
    ]),
    ("Bank 2  0x08100000~0x081FFFFF  RWW-safe for app", 0x08200000, [
        ("SLOT0",    0x08100000, 448, "B2 0-55",    "#e76f51", "TAG 1K + image"),
        ("SLOT1",    0x08170000, 448, "B2 56-111",  "#f4a261", "TAG 1K + image"),
        ("BOOT_LOG", 0x081E0000,  16, "B2 112-113", "#8367c7", "8K x 2 ping-pong"),
        ("NVS",      0x081E4000, 112, "B2 114-127", "#6c757d", "virtual EEPROM (reserved)"),
    ]),
]
TAGS = [
  ("0x000", "firm_tag_t   magic | fw_addr | fw_size | fw_crc", "16 B", "(3) COMMIT - written last, atomic", "#c1121f"),
  ("0x010", "firm_tag_t   tag_crc + padding",                   "16 B", "(2)",                              "#e07a5f"),
  ("0x020", "boot_slot_t  magic | seq | flags | crc",           "16 B", "(1) written first",                "#8367c7"),
  ("0x030", "invalid marker",                                   "16 B", "written on slot invalidate",       "#4a5568"),
  ("0x040", "reserved",                                        "960 B", "",                                 "#8d99ae"),
]

PX_PER_KB, COL_W, GAP, LEFT, TOP, MIN_H = 0.40, 470, 56, 34, 84, 30
esc = lambda s: s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

body, col_bottom = [], 0
for ci, (title, end_addr, blocks) in enumerate(BANKS):
    x = LEFT + ci * (COL_W + GAP)
    body.append(f'<text x="{x}" y="{TOP-26}" class="bank">{esc(title)}</text>')
    y = TOP
    for name, addr, kb, sect, color, note in blocks:
        h = max(MIN_H, kb * PX_PER_KB)
        body.append(f'<rect x="{x}" y="{y:.1f}" width="{COL_W}" height="{h:.1f}" '
                    f'fill="{color}" stroke="#11151a" stroke-width="1.4" rx="4"/>')
        if h >= 62:
            rows = [(24, name, f"{kb} KB", "nm", "sz"),
                    (43, f"0x{addr:08X}", sect, "ad", "ad"),
                    (61, note, "", "nt", "nt")]
            for dy, l, r, cl, cr in rows:
                if l: body.append(f'<text x="{x+16}" y="{y+dy:.1f}" class="{cl}">{esc(l)}</text>')
                if r: body.append(f'<text x="{x+COL_W-16}" y="{y+dy:.1f}" class="{cr}" text-anchor="end">{esc(r)}</text>')
        elif h >= 44:
            body.append(f'<text x="{x+16}" y="{y+20:.1f}" class="nm">{esc(name)}</text>')
            body.append(f'<text x="{x+COL_W-16}" y="{y+20:.1f}" class="sz" text-anchor="end">{kb} KB</text>')
            body.append(f'<text x="{x+16}" y="{y+37:.1f}" class="ad">0x{addr:08X}</text>')
            body.append(f'<text x="{x+COL_W-16}" y="{y+37:.1f}" class="ad" text-anchor="end">{esc(sect)}</text>')
        else:
            body.append(f'<text x="{x+16}" y="{y+h/2+5:.1f}" class="nm2">{esc(name)}</text>')
            body.append(f'<text x="{x+150}" y="{y+h/2+5:.1f}" class="ad">0x{addr:08X}</text>')
            body.append(f'<text x="{x+COL_W-16}" y="{y+h/2+5:.1f}" class="ad" text-anchor="end">{kb} KB  ·  {esc(sect)}</text>')
        y += h
    body.append(f'<text x="{x}" y="{y+17:.1f}" class="adx">0x{end_addr:08X}</text>')
    col_bottom = max(col_bottom, y + 17)

iy, W_ALL = col_bottom + 54, COL_W * 2 + GAP
body.append(f'<text x="{LEFT}" y="{iy-18}" class="bank">Slot / FIRM  TAG area (first 1 KB)  -  write order</text>')
ty = iy
for off, label, size, note, color in TAGS:
    body.append(f'<rect x="{LEFT}" y="{ty}" width="{W_ALL}" height="30" fill="{color}" stroke="#11151a" stroke-width="1.2" rx="4"/>')
    body.append(f'<text x="{LEFT+16}" y="{ty+20}" class="tgo">{off}</text>')
    body.append(f'<text x="{LEFT+80}" y="{ty+20}" class="tg">{esc(label)}</text>')
    body.append(f'<text x="{LEFT+W_ALL-16}" y="{ty+20}" class="tg" text-anchor="end">{esc(size)}   {esc(note)}</text>')
    ty += 33

H, W = ty + 26, LEFT * 2 + W_ALL
svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="ui-monospace,SFMono-Regular,Menlo,Consolas,monospace">
<style>
  .bank{{font-size:15px;font-weight:700;fill:#6b7684}}
  .nm {{font-size:16px;font-weight:700;fill:#ffffff}}
  .nm2{{font-size:13px;font-weight:700;fill:#ffffff}}
  .sz {{font-size:13px;font-weight:600;fill:#ffffff}}
  .ad {{font-size:11.5px;fill:#ffffff;opacity:.88}}
  .nt {{font-size:11.5px;fill:#ffffff;opacity:.80}}
  .tgo{{font-size:12px;font-weight:700;fill:#ffffff}}
  .tg {{font-size:12px;fill:#ffffff}}
  .adx{{font-size:11.5px;fill:#7d8590}}
  @media (prefers-color-scheme: dark){{ .bank{{fill:#adb9c7}} .adx{{fill:#8b949e}} }}
</style>
{chr(10).join(body)}
</svg>'''
open("memory-map.svg", "w").write(svg)
print(f"memory-map.svg  {W}x{H:.0f}  {len(svg)} bytes")
