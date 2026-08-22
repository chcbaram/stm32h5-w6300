"""부트로더 cmd 패킷 프로토콜 클라이언트.

전송계층과 무관하다. CDC(시리얼)든 HID든 read/write 만 갈아끼우면 된다
(펌웨어 쪽 cmd.c 도 같은 구조다).

패킷 포맷 (little endian)
  STX0(0x02) STX1(0xFD) type cmd_l cmd_h err_l err_h len_l len_h [data...] checksum
  checksum = (~sum(header+data)) + 1
"""
import struct, time

STX0, STX1 = 0x02, 0xFD

PKT_TYPE_CMD   = 0x00
PKT_TYPE_RESP  = 0x01

BOOT_CMD_INFO      = 0x0000
BOOT_CMD_VERSION   = 0x0001
BOOT_CMD_FW_BEGIN  = 0x0002
BOOT_CMD_FW_ERASE  = 0x0003
BOOT_CMD_FW_WRITE  = 0x0004
BOOT_CMD_FW_READ   = 0x0005
BOOT_CMD_FW_END    = 0x0006
BOOT_CMD_FW_VERIFY = 0x0007
BOOT_CMD_FW_UPDATE = 0x0008
BOOT_CMD_FW_JUMP   = 0x0009
BOOT_CMD_LOG_COUNT = 0x000A
BOOT_CMD_LOG_READ  = 0x000B


def build(cmd, data=b"", type_=PKT_TYPE_CMD, err=0):
    head = bytes([STX0, STX1, type_]) + struct.pack("<HHH", cmd, err, len(data))
    body = head + data
    return body + bytes([((~sum(body)) + 1) & 0xFF])


class CmdChannel:
    """read(n)/write(b) 두 개만 제공하면 되는 전송계층 위의 클라이언트."""

    def __init__(self, transport):
        self.t = transport

    def request(self, cmd, data=b"", timeout=3.0):
        self.t.flush_input()
        self.t.write(build(cmd, data))

        buf = b""
        t0 = time.time()
        while time.time() - t0 < timeout:
            buf += self.t.read(512)
            # STX 를 찾아 정렬한다. CLI 로그가 섞여 들어올 수 있다.
            i = buf.find(bytes([STX0, STX1]))
            if i < 0:
                continue
            if len(buf) - i < 9:
                continue
            _, _, typ, rcmd, err, ln = struct.unpack("<BBBHHH", buf[i:i+9])
            if len(buf) - i < 9 + ln + 1:
                continue
            payload = buf[i+9 : i+9+ln]
            return {"type": typ, "cmd": rcmd, "err": err, "data": payload}
        raise TimeoutError(f"cmd 0x{cmd:04X} 응답 없음 (받은 {len(buf)}B)")


class SerialTransport:
    def __init__(self, ser):
        self.ser = ser

    def flush_input(self):
        self.ser.reset_input_buffer()

    def write(self, b):
        self.ser.write(b); self.ser.flush()

    def read(self, n):
        # ser.read(n) 은 n 바이트가 다 찰 때까지 기다린다. 응답은 보통 10바이트뿐이라
        # 매 요청마다 타임아웃을 통째로 까먹는다(실측 2.4 KB/s -> 아래 방식으로 개선).
        # 1바이트만 기다린 뒤 버퍼에 있는 것을 몰아 읽는다.
        first = self.ser.read(1)
        if not first:
            return b""
        n_wait = self.ser.in_waiting
        return first + (self.ser.read(n_wait) if n_wait else b"")


class HidTransport:
    """WebHID 와 동일한 방식의 HID 채널.

    HID 는 스트림이 아니라 64바이트 고정 리포트 단위다. 펌웨어(cmd_hid.c)와 같은
    규약을 쓴다.
        리포트[0]  = 유효 바이트 수 (1~63)
        리포트[1:] = 페이로드
    """
    RPT = 64
    PAYLOAD = RPT - 1

    def __init__(self, vid=0xCAFE, pid=0xB003):
        import hid
        self.dev = hid.device()
        self.dev.open(vid, pid)
        self.dev.set_nonblocking(1)
        self.buf = bytearray()

    def close(self):
        self.dev.close()

    def flush_input(self):
        while self.dev.read(self.RPT):
            pass
        self.buf.clear()

    def write(self, b):
        for i in range(0, len(b), self.PAYLOAD):
            chunk = b[i:i+self.PAYLOAD]
            rpt = bytes([0x00, len(chunk)]) + chunk       # [0]=report id
            rpt += b"\x00" * (self.RPT + 1 - len(rpt))
            self.dev.write(rpt)

    def read(self, n):
        t0 = time.time()
        while not self.buf and time.time() - t0 < 0.3:
            d = self.dev.read(self.RPT)
            if d:
                ln = d[0]
                if 0 < ln <= self.PAYLOAD:
                    self.buf += bytes(d[1:1+ln])
            else:
                time.sleep(0.001)
        out = bytes(self.buf[:n]) if n else bytes(self.buf)
        del self.buf[:len(out)]
        return out


DEV_MODE_BOOT = 0
DEV_MODE_APP  = 1

def parse_info(d):
    """연결 직후 가장 먼저 부른다.

    부트로더와 앱이 같은 VID/PID 로 열거되므로 USB 만으로는 구분할 수 없다.
    mode 로 판별한다.
    """
    f = struct.unpack("<8I", d[:32])
    out = dict(zip(("magic","mode","boot_addr","firm_addr","firm_size",
                    "slot_size","slot_max","family_id"), f))
    z = lambda b: b.split(b"\0")[0].decode("ascii", "replace").strip()
    out["name"]    = z(d[32:64])
    out["version"] = z(d[64:96])
    out["mode_str"] = "BOOT" if out["mode"] == DEV_MODE_BOOT else "APP"
    return out


ITEM_FMT = "<BBHIIII32s32s"
ITEM_SZ  = struct.calcsize(ITEM_FMT)

def _item(d):
    v, idx, _, addr, seq, sz, crc, name, ver = struct.unpack(ITEM_FMT, d[:ITEM_SZ])
    z = lambda s: s.split(b"\0")[0].decode("ascii", "replace").strip()
    return dict(valid=bool(v), index=idx, addr=addr, seq=seq,
                fw_size=sz, fw_crc=crc, name=z(name), version=z(ver))

def parse_version(d, slot_max=2):
    out = {"firm": _item(d)}
    out["slot"] = [_item(d[ITEM_SZ*(i+1):]) for i in range(slot_max)]
    w, p, r, _ = struct.unpack("<bbbb", d[ITEM_SZ*(slot_max+1):ITEM_SZ*(slot_max+1)+4])
    out.update(write_slot=w, pending_slot=p, rollback_slot=r)
    return out
