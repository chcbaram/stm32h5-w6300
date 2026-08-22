#!/usr/bin/env python3
"""부트 로그 모니터. 부팅 배너와 reset_count 를 각각 타임스탬프와 함께 센다.

  python3 monitor.py [초]

배너는 매 부팅마다 출력되지만 reset_count 는 더블클릭 대기(300ms) '뒤'에
출력되므로, 대기 중에 리셋된 부팅은 배너만 보이고 reset_count 는 안 보인다.
두 개수를 비교하면 바운스/연속리셋을 구분할 수 있다.
"""
import sys, time, re, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib.cli import Cli

dur = float(sys.argv[1]) if len(sys.argv) > 1 else 30.0
with Cli() as c:
    c.ser.reset_input_buffer()
    print(f">>> {dur:.0f}초간 캡처", flush=True)
    buf = ""; t0 = time.time()
    n_banner = n_count = 0
    while time.time() - t0 < dur:
        buf += c.ser.read(4096).decode("utf-8", "replace")
        while True:
            ib = buf.find("[ Bootloader Begin")
            ic = buf.find("reset_count :")
            if ib < 0 and ic < 0:
                break
            if ic < 0 or (0 <= ib < ic):
                j = buf.find("\n", ib)
                if j < 0: break
                n_banner += 1
                print(f"  [{time.time()-t0:5.1f}s] BOOT #{n_banner}", flush=True)
                buf = buf[j:]
            else:
                j = buf.find("\n", ic)
                if j < 0: break
                bits = sorted(set(re.findall(r"RESET_BIT_\w+", buf[:ic][-300:])))
                n_count += 1
                print(f"  [{time.time()-t0:5.1f}s]   -> {buf[ic:j].strip()}  bits={bits}", flush=True)
                buf = buf[j:]
    print(f"\n부팅 배너 {n_banner}회 / reset_count 출력 {n_count}회")
