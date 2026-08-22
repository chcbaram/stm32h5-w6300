#!/usr/bin/env python3
"""LAN 안의 보드를 찾는다. 그리고 가짜 보드를 띄운다.

  python3 discover.py                 # 스캔만
  python3 discover.py --serve         # 가짜 보드로 응답만 (보드 스캔 시험용)
  python3 discover.py --serve --name FAKE-BOARD-1 --ip 192.168.0.99

펌웨어 쪽은 src/ap/modules/net/net_disc.c 다. 규약은 UDP 브로드캐스트 비컨이고
질의/응답이 같은 60바이트 구조체를 쓴다(매직만 다르다).

보드가 하나뿐일 때 스캔이 되는지 보려면 --serve 로 가짜 보드를 띄운다.
"""
import argparse
import socket
import struct
import sys
import uuid

PORT      = 5300
MAGIC_REQ = 0x51445242      # "BRDQ"
MAGIC_RSP = 0x52445242      # "BRDR"
VER       = 1

# magic, ver, mode, rsv[2], ip[4], mac[6], rsv2[2], name[24], version[16]
FMT  = "<IBB2s4s6s2s24s16s"
SIZE = struct.calcsize(FMT)     # 60

MODE = {0: "BOOT", 1: "APP"}


def pack(magic, mode, ip, mac, name, version):
    return struct.pack(FMT, magic, VER, mode, b"\0" * 2,
                       bytes(int(x) for x in ip.split(".")), mac, b"\0" * 2,
                       name.encode()[:23], version.encode()[:15])


def unpack(data):
    magic, ver, mode, _, ip, mac, _, name, version = struct.unpack(FMT, data[:SIZE])
    return {
        "magic": magic, "ver": ver, "mode": mode,
        "ip":  ".".join(str(b) for b in ip),
        "mac": ":".join(f"{b:02X}" for b in mac),
        "name":    name.split(b"\0")[0].decode("utf-8", "replace"),
        "version": version.split(b"\0")[0].decode("utf-8", "replace"),
    }


def my_ip():
    """기본 경로로 나가는 인터페이스의 주소. 실제로 보내지는 않는다."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 1))
        return s.getsockname()[0]
    except OSError:
        return "0.0.0.0"
    finally:
        s.close()


def broadcast_addrs():
    """255.255.255.255 하나로는 인터페이스가 여럿일 때 한 곳으로만 나가는 OS 가
    있다. 붙어 있는 서브넷의 브로드캐스트 주소도 같이 던진다(NU87 discover.py
    에서 가져온 방식)."""
    addrs = ["255.255.255.255"]
    ip = my_ip()
    if not ip.startswith("127.") and ip != "0.0.0.0":
        addrs.append(ip.rsplit(".", 1)[0] + ".255")
    return list(dict.fromkeys(addrs))


def do_scan(timeout):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(timeout)

    req = pack(MAGIC_REQ, 1, my_ip(), uuid.getnode().to_bytes(6, "big"), "HOST", "-")
    for addr in broadcast_addrs():
        try:
            sock.sendto(req, (addr, PORT))
        except OSError:
            pass

    found = {}
    while True:
        try:
            data, src = sock.recvfrom(512)
        except socket.timeout:
            break
        if len(data) < SIZE:
            continue
        b = unpack(data)
        if b["magic"] != MAGIC_RSP or b["ver"] != VER:
            continue
        found[b["ip"] if b["ip"] != "0.0.0.0" else src[0]] = b

    sock.close()
    return found


def do_serve(name, version, ip, mode):
    """가짜 보드. 질의가 오면 응답한다."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.bind(("", PORT))

    mac = uuid.uuid4().bytes[:6]
    rsp = pack(MAGIC_RSP, mode, ip, mac, name, version)

    print(f"가짜 보드 : {name} {version}  {ip}  {':'.join(f'{b:02X}' for b in mac)}")
    print(f"UDP {PORT} 대기 중. Ctrl-C 로 종료.")

    while True:
        try:
            data, src = sock.recvfrom(512)
        except KeyboardInterrupt:
            break
        if len(data) < SIZE:
            continue
        b = unpack(data)
        if b["magic"] != MAGIC_REQ or b["ver"] != VER:
            continue
        print(f"  질의 <- {src[0]}:{src[1]}  ({b['name']})   -> 응답")
        sock.sendto(rsp, src)

    sock.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--serve",   action="store_true", help="가짜 보드로 응답만 한다")
    ap.add_argument("--name",    default="FAKE-BOARD")
    ap.add_argument("--version", default="V000000R0")
    ap.add_argument("--ip",      default=None, help="가짜 보드가 알릴 IP (기본: 이 PC)")
    ap.add_argument("--mode",    type=int, default=1, help="0=BOOT, 1=APP")
    ap.add_argument("--timeout", type=float, default=2.0)
    args = ap.parse_args()

    if args.serve:
        do_serve(args.name, args.version, args.ip or my_ip(), args.mode)
        return

    found = do_scan(args.timeout)
    if not found:
        print("찾은 보드가 없다.")
        sys.exit(1)

    print(f"{len(found)} 대\n")
    print(f"{'IP':<16} {'MODE':<5} {'NAME':<24} {'VERSION':<16} MAC")
    for ip, b in sorted(found.items()):
        print(f"{ip:<16} {MODE.get(b['mode'], '?'):<5} "
              f"{b['name']:<24} {b['version']:<16} {b['mac']}")


if __name__ == "__main__":
    main()
