#!/usr/bin/env python3
"""cmd 패킷 프로토콜로 펌웨어를 다운로드한다 (CDC 채널).

  python3 download.py <fw.bin> [포트]

UF2 드래그&드롭과 같은 슬롯에 같은 태그 포맷으로 기록된다.
"""
import sys, os, time, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import serial
from lib.cmdproto import *

CHUNK = 512          # cmd 최대 데이터 1024B, offset 4B 포함

def main():
    path = sys.argv[1]
    port = sys.argv[2] if len(sys.argv) > 2 else "/dev/cu.usbmodem1412301"

    fw = open(path, "rb").read()
    if len(fw) % 16:
        fw += b"\xFF" * (16 - len(fw) % 16)

    s = serial.Serial(port, 115200, timeout=0.2)
    time.sleep(0.4)
    ch = CmdChannel(SerialTransport(s))

    r = ch.request(BOOT_CMD_FW_BEGIN, struct.pack("<I", len(fw)))
    if r["err"]:
        print(f"FW_BEGIN 실패 err=0x{r['err']:04X}"); return 1
    slot = r["data"][0]
    print(f"slot{slot} 에 {len(fw)}바이트 ({len(fw)/1024:.1f} KB)")

    t0 = time.time()
    r = ch.request(BOOT_CMD_FW_ERASE, timeout=10.0)
    if r["err"]:
        print(f"FW_ERASE 실패 err=0x{r['err']:04X}"); return 1
    print(f"  소거 {time.time()-t0:.1f}s")

    t0 = time.time()
    for off in range(0, len(fw), CHUNK):
        blk = fw[off:off+CHUNK]
        r = ch.request(BOOT_CMD_FW_WRITE, struct.pack("<I", off) + blk)
        if r["err"]:
            print(f"\nFW_WRITE off=0x{off:X} 실패 err=0x{r['err']:04X}"); return 1
        if (off // CHUNK) % 20 == 0:
            pct = off * 100 // len(fw)
            print(f"\r  기록 {pct:3d}%", end="", flush=True)
    dt = time.time() - t0
    print(f"\r  기록 100%  {dt:.1f}s  ({len(fw)/dt/1024:.1f} KB/s)")

    r = ch.request(BOOT_CMD_FW_END, timeout=10.0)
    print(f"  FW_END err=0x{r['err']:04X}")
    if r["err"]:
        return 1

    r = ch.request(BOOT_CMD_FW_VERIFY, bytes([slot]), timeout=10.0)
    print(f"  FW_VERIFY err=0x{r['err']:04X}")
    s.close()
    return 0 if r["err"] == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
