"""실제 펌웨어 갱신. UF2(MSC) 와 cmd 다운로드(HID) 두 경로."""
import glob, os, re, shutil, struct, subprocess, time
import pytest
from lib.cmdproto import (BOOT_CMD_VERSION, BOOT_CMD_FW_BEGIN, BOOT_CMD_FW_ERASE,
                          BOOT_CMD_FW_WRITE, BOOT_CMD_FW_END, BOOT_CMD_FW_VERIFY,
                          parse_version)

VOLUME = "/Volumes/H5BOOT"


def _slot_state(boot):
    out = boot.cmd("boot info", 2.0)
    slots = re.findall(r"SLOT(\d)\s+0x\w+\s+valid=(\d)\s+seq=(\d+)\s+size=(\d+)\s+crc=(0x\w+)", out)
    write = int(re.search(r"write slot\s*:\s*(-?\d+)", out).group(1))
    return {int(i): dict(valid=v == "1", seq=int(s), size=int(sz), crc=int(c, 16))
            for i, v, s, sz, c in slots}, write


def test_msc_volume_mounted(boot):
    for _ in range(20):
        if os.path.exists(VOLUME):
            break
        time.sleep(0.5)
    assert os.path.exists(VOLUME), "H5BOOT 볼륨이 마운트되지 않았다"

    readme = os.path.join(VOLUME, "README.TXT")
    assert os.path.exists(readme)
    txt = open(readme, encoding="utf-8", errors="replace").read()
    assert "STM32H5-W6300-BOOT" in txt
    assert "FamilyID: 0xFFFF0003" in txt
    assert "SLOT0" in txt and "SLOT1" in txt


def test_uf2_drop_updates(boot, app_uf2):
    """UF2 를 떨어뜨리면 낡은 슬롯에 기록되고 FIRM 에 적용된 뒤 앱이 뜬다."""
    if not os.path.exists(VOLUME):
        pytest.skip("H5BOOT 볼륨이 없다")

    before, write_slot = _slot_state(boot)
    boot.ser.reset_input_buffer()

    shutil.copy(app_uf2, VOLUME)

    log = ""
    t0 = time.time()
    while time.time() - t0 < 25:
        log += boot.read_for(1.0)
        if "Firmware Begin" in log:
            break

    assert "uf2 begin" in log, f"UF2 수신이 시작되지 않았다:\n{log[-500:]}"
    m = re.search(r"uf2 begin -> slot(\d)", log)
    assert m and int(m.group(1)) == write_slot, \
        f"write slot({write_slot}) 이 아닌 slot{m.group(1) if m else '?'} 에 기록됐다"
    assert "uf2 flush" in log, f"태그 기록이 없다:\n{log[-500:]}"
    assert "bootApplySlot" in log, f"FIRM 적용이 없다:\n{log[-500:]}"
    assert "Firmware Begin" in log, f"앱이 부팅하지 않았다:\n{log[-500:]}"
    assert "[E_]" not in log, f"에러 로그가 있다:\n{log}"


def test_hid_download(hid, app_bin):
    """cmd 프로토콜로 슬롯까지만 기록한다(적용/점프는 하지 않는다)."""
    fw = open(app_bin, "rb").read()
    if len(fw) % 16:
        fw += b"\xFF" * (16 - len(fw) % 16)

    r = hid.request(BOOT_CMD_FW_BEGIN, struct.pack("<I", len(fw)))
    assert r["err"] == 0, f"FW_BEGIN err=0x{r['err']:04X}"
    slot = r["data"][0]

    assert hid.request(BOOT_CMD_FW_ERASE, timeout=20)["err"] == 0

    t0 = time.time()
    CHUNK = 512
    for off in range(0, len(fw), CHUNK):
        r = hid.request(BOOT_CMD_FW_WRITE, struct.pack("<I", off) + fw[off:off+CHUNK])
        assert r["err"] == 0, f"FW_WRITE off=0x{off:X} err=0x{r['err']:04X}"
    dt = time.time() - t0

    assert hid.request(BOOT_CMD_FW_END, timeout=20)["err"] == 0
    assert hid.request(BOOT_CMD_FW_VERIFY, bytes([slot]), timeout=20)["err"] == 0

    v = parse_version(hid.request(BOOT_CMD_VERSION)["data"])
    assert v["slot"][slot]["valid"], f"slot{slot} 이 유효하지 않다"
    print(f"\n  HID 다운로드 {len(fw)/1024:.1f} KB  {dt:.1f}s  ({len(fw)/dt/1024:.0f} KB/s)")
