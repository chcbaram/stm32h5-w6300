"""firm_tag_t / boot_slot_t 생성기.

CRC 는 부트로더의 utilUpdateCrc() 와 동일해야 한다.
  i   = ((crc >> 8) ^ data) & 0xFF
  crc = ((crc << 8) ^ table[i]) & 0xFFFF
테이블은 poly 0x8005, MSB-first, init 0x0000 (CRC-16/BUYPASS).
"""
import struct

TAG_MAGIC       = 0x54414720   # "TAG "
VERSION_MAGIC   = 0x56455220   # "VER "
BOOT_SLOT_MAGIC = 0x534C4F54   # "SLOT"
FLASH_SIZE_TAG  = 0x400


def _mktable(poly=0x8005):
    t = []
    for i in range(256):
        c = (i << 8) & 0xFFFF
        for _ in range(8):
            c = ((c << 1) ^ poly) & 0xFFFF if (c & 0x8000) else (c << 1) & 0xFFFF
        t.append(c)
    return t


_TABLE = _mktable()


def crc16(data, crc=0):
    for b in data:
        i = ((crc >> 8) ^ b) & 0xFF
        crc = ((crc << 8) ^ _TABLE[i]) & 0xFFFF
    return crc


def make_tag(fw_bin: bytes) -> bytes:
    """1KB TAG 영역 전체를 만든다 (firm_tag_t + boot_slot_t 자리는 비움)."""
    fw_crc = crc16(fw_bin)
    tag = struct.pack("<IIII", TAG_MAGIC, FLASH_SIZE_TAG, len(fw_bin), fw_crc)
    tag_crc = crc16(tag)
    tag += struct.pack("<I", tag_crc)
    return tag.ljust(FLASH_SIZE_TAG, b"\xFF"), fw_crc


def make_slot_meta(seq: int) -> bytes:
    """boot_slot_t 16바이트 (TAG 영역 오프셋 0x20)."""
    body = struct.pack("<III", BOOT_SLOT_MAGIC, seq, 0xFFFFFFFF)
    return body + struct.pack("<I", crc16(body))
