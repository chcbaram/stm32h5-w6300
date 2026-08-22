#!/usr/bin/env python3
"""부트로더 cmd 패킷 프로토콜로 펌웨어를 다운로드한다.

  python3 download.py [fw.bin] [--port PORT] [--hid] [--no-jump]

기본값은 build/stm32h5-w6300-fw.bin 이고 CDC(시리얼) 채널을 쓴다.
--hid 를 주면 WebHID 와 같은 HID 채널로 보낸다(hidapi 필요).

UF2 드래그&드롭과 같은 슬롯에 같은 태그 포맷으로 기록되므로, 슬롯 핑퐁과
롤백이 동일하게 동작한다.
"""
import argparse, glob, os, struct, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cmdproto import (CmdChannel, SerialTransport, HidTransport,
                      BOOT_CMD_FW_BEGIN, BOOT_CMD_FW_ERASE, BOOT_CMD_FW_WRITE,
                      BOOT_CMD_FW_END, BOOT_CMD_FW_VERIFY, BOOT_CMD_FW_UPDATE,
                      BOOT_CMD_VERSION, parse_version)

CHUNK = 512      # cmd 최대 데이터 1024B. offset 4B 를 빼고도 여유가 있다.
ERR_BOOT_NO_PENDING = 0x0016   # 적용할 것 없음(슬롯 == FIRM)
CMD_BAUD = 921600   # 115200 이 아니면 무엇이든 된다. USB CDC 라 실제 속도와 무관하다.


def find_default_bin():
    here = os.path.dirname(os.path.abspath(__file__))
    cand = glob.glob(os.path.join(here, "..", "..", "build", "*.bin"))
    return os.path.normpath(cand[0]) if cand else None


def open_channel(args):
    if args.hid:
        return CmdChannel(HidTransport(vid=0xCAFE, pid=0xB003))

    import serial
    port = args.port
    if port is None:
        ports = sorted(glob.glob("/dev/cu.usbmodem*"))
        if not ports:
            sys.exit("시리얼 포트를 찾지 못했다. --port 로 지정한다.")
        port = ports[-1]
        print(f"포트 자동 선택: {port}")
    # 115200 은 "터미널" 로 예약되어 있다. 그 보율로 열면 펌웨어가 CDC 를 CLI 에
    # 넘겨버려 cmd 패킷을 아무도 읽지 않는다. 다른 보율로 열어 cmd 가 독점하게 한다.
    # (firmware cdcGetType() / cmd_task.c cmdChIsEnabled())
    ser = serial.Serial(port, CMD_BAUD, timeout=0.3)
    time.sleep(0.4)
    return CmdChannel(SerialTransport(ser))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("binary", nargs="?", default=None)
    ap.add_argument("--port", default=None, help="CDC 시리얼 포트")
    ap.add_argument("--hid", action="store_true", help="HID 채널 사용")
    ap.add_argument("--no-jump", action="store_true", help="적용/점프 없이 슬롯 기록까지만")
    args = ap.parse_args()

    path = args.binary or find_default_bin()
    if not path or not os.path.exists(path):
        sys.exit("펌웨어 .bin 을 찾지 못했다. 먼저 빌드한다.")

    fw = open(path, "rb").read()
    if len(fw) % 16:
        fw += b"\xFF" * (16 - len(fw) % 16)      # 쿼드워드 정렬

    ch = open_channel(args)

    r = ch.request(BOOT_CMD_VERSION)
    v = parse_version(r["data"])
    print(f"현재 FIRM : {v['firm']['name']} {v['firm']['version']}  "
          f"(seq {v['firm']['seq']}, crc 0x{v['firm']['fw_crc']:04X})")

    r = ch.request(BOOT_CMD_FW_BEGIN, struct.pack("<I", len(fw)))
    if r["err"]:
        sys.exit(f"FW_BEGIN 실패 err=0x{r['err']:04X}")
    slot = r["data"][0]
    print(f"{os.path.basename(path)}  {len(fw)/1024:.1f} KB  ->  slot{slot}")

    t0 = time.time()
    r = ch.request(BOOT_CMD_FW_ERASE, timeout=15.0)
    if r["err"]:
        sys.exit(f"FW_ERASE 실패 err=0x{r['err']:04X}")
    print(f"  소거   {time.time()-t0:.1f}s")

    t0 = time.time()
    for off in range(0, len(fw), CHUNK):
        r = ch.request(BOOT_CMD_FW_WRITE, struct.pack("<I", off) + fw[off:off+CHUNK])
        if r["err"]:
            sys.exit(f"\nFW_WRITE off=0x{off:X} 실패 err=0x{r['err']:04X}")
        if (off // CHUNK) % 16 == 0:
            print(f"\r  기록   {off*100//len(fw):3d}%", end="", flush=True)
    dt = time.time() - t0
    print(f"\r  기록   100%  {dt:.1f}s  ({len(fw)/dt/1024:.0f} KB/s)")

    r = ch.request(BOOT_CMD_FW_END, timeout=15.0)
    if r["err"]:
        sys.exit(f"FW_END 실패 err=0x{r['err']:04X}")

    r = ch.request(BOOT_CMD_FW_VERIFY, bytes([slot]), timeout=15.0)
    if r["err"]:
        sys.exit(f"FW_VERIFY 실패 err=0x{r['err']:04X}")
    print("  검증   OK")

    if args.no_jump:
        print("  (--no-jump : 적용하지 않았다. 부트로더가 다음 부팅에 적용한다)")
        return

    # 응답을 확인한다. 슬롯 내용이 지금 FIRM 과 같으면 부트로더가 할 일이 없어서
    # 리셋해도 아무 일이 없다. 그걸 "적용됐다" 고 말하면 안 된다.
    try:
        r = ch.request(BOOT_CMD_FW_UPDATE, timeout=5.0)
    except TimeoutError:
        r = None                      # 적용을 시작하며 리셋한 경우다. 정상.

    if r and r["err"] == ERR_BOOT_NO_PENDING:
        print("  적용   할 것이 없다. 슬롯 이미지가 지금 실행 중인 것과 같다.")
        return
    if r and r["err"]:
        sys.exit(f"FW_UPDATE 실패 err=0x{r['err']:04X}")

    print("  적용   요청 완료. 부트로더가 FIRM 에 복사한 뒤 재부팅한다.")


if __name__ == "__main__":
    main()
