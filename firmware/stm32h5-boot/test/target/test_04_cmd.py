"""cmd 패킷 프로토콜. CDC 와 HID 가 같은 커맨드 셋을 쓴다."""
import struct
import pytest
from lib.cmdproto import (BOOT_CMD_INFO, BOOT_CMD_VERSION, BOOT_CMD_LOG_COUNT,
                          parse_info, parse_version, DEV_MODE_BOOT)


def test_hid_info(hid):
    i = parse_info(hid.request(BOOT_CMD_INFO)["data"])
    assert i["mode"] == DEV_MODE_BOOT, "부트로더 모드여야 한다"
    assert i["name"].endswith("BOOT"), f"이름이 이상하다: {i['name']}"
    assert i["boot_addr"] == 0x08000000
    assert i["firm_addr"] == 0x08020000
    assert i["slot_max"] == 2
    assert i["family_id"] == 0xFFFF0003


def test_hid_version(hid):
    r = hid.request(BOOT_CMD_VERSION)
    assert r["err"] == 0
    v = parse_version(r["data"])
    assert v["firm"]["valid"], "FIRM 이 유효해야 한다"
    assert len(v["slot"]) == 2
    assert -1 <= v["write_slot"] < 2


def test_hid_log_count(hid):
    r = hid.request(BOOT_CMD_LOG_COUNT)
    assert r["err"] == 0
    (n,) = struct.unpack("<H", r["data"])
    assert 0 <= n <= 512


def test_unknown_command_rejected(hid):
    r = hid.request(0x7FFF)
    assert r["err"] != 0, "모르는 커맨드는 에러를 돌려줘야 한다"


def test_cdc_matches_hid(boot, hid):
    """같은 커맨드가 두 채널에서 같은 값을 돌려줘야 한다."""
    import glob, time, serial
    from lib.cmdproto import CmdChannel, SerialTransport

    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if len(ports) < 2:
        pytest.skip("CDC 포트를 찾지 못했다")

    hid_info = parse_info(hid.request(BOOT_CMD_INFO)["data"])

    for p in reversed(ports):
        try:
            s = serial.Serial(p, 115200, timeout=0.3)
        except Exception:
            continue
        time.sleep(0.3)
        try:
            cdc_info = parse_info(CmdChannel(SerialTransport(s)).request(BOOT_CMD_INFO, timeout=2.0)["data"])
            s.close()
        except Exception:
            s.close()
            continue

        assert cdc_info["mode"]      == hid_info["mode"]
        assert cdc_info["firm_addr"] == hid_info["firm_addr"]
        assert cdc_info["name"]      == hid_info["name"]
        return

    pytest.skip("CDC 채널에서 응답을 받지 못했다")
