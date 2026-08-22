#!/usr/bin/env python3
"""앱 .bin 에 TAG(+slot meta)를 붙여 FIRM/SLOT 에 그대로 구울 이미지를 만든다.

  python3 mkimage.py <app.bin> <out.bin> [seq]
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib.firmtag import make_tag, make_slot_meta, FLASH_SIZE_TAG

src, dst = sys.argv[1], sys.argv[2]
seq = int(sys.argv[3]) if len(sys.argv) > 3 else 1

fw = open(src, "rb").read()
if len(fw) % 16:
    fw += b"\xFF" * (16 - len(fw) % 16)      # 16B 정렬로 패딩

tag, fw_crc = make_tag(fw)
tag = bytearray(tag)
tag[0x20:0x30] = make_slot_meta(seq)          # boot_slot_t

open(dst, "wb").write(bytes(tag) + fw)
print(f"{dst}: fw_size={len(fw)} ({len(fw)/1024:.1f} KB)  fw_crc=0x{fw_crc:04X}  seq={seq}")
print(f"  전체 = TAG {FLASH_SIZE_TAG} + 이미지 {len(fw)} = {FLASH_SIZE_TAG+len(fw)} bytes")
