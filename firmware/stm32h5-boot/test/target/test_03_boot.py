"""부트 정보 / 슬롯 판정 / 부트 로그."""
import re


def test_boot_info(boot):
    out = boot.cmd("boot info", 2.0)
    assert "STM32H5-W6300-BOOT" in out
    assert "FIRM" in out and "SLOT0" in out and "SLOT1" in out
    for k in ("write slot", "pending slot", "rollback slot", "next seq"):
        assert k in out, f"{k} 가 없다"


def test_firm_valid(boot):
    """FIRM 이 유효해야 이후 시험이 의미가 있다."""
    out = boot.cmd("boot info", 2.0)
    m = re.search(r"FIRM\s+0x[0-9A-Fa-f]+\s+valid=(\d)", out)
    assert m and m.group(1) == "1", f"FIRM 이 유효하지 않다:\n{out}"


def test_verify_all(boot):
    out = boot.cmd("boot verify", 2.5)
    m = re.search(r"FIRM\s*:\s*err\s*(0x[0-9A-Fa-f]+)", out)
    assert m and int(m.group(1), 16) == 0, f"FIRM 검증 실패:\n{out}"


def test_slot_roles_consistent(boot):
    """핑퐁 규칙 : FIRM 과 일치하는 슬롯은 보존되고, 낡은 쪽이 수신 대상이다."""
    out = boot.cmd("boot info", 2.0)

    firm = re.search(r"FIRM\s+0x\w+\s+valid=(\d)\s+seq=(\d+)\s+size=(\d+)\s+crc=(0x\w+)", out)
    slots = re.findall(r"SLOT(\d)\s+0x\w+\s+valid=(\d)\s+seq=(\d+)\s+size=(\d+)\s+crc=(0x\w+)", out)
    write = int(re.search(r"write slot\s*:\s*(-?\d+)", out).group(1))
    assert firm and len(slots) == 2

    f_size, f_crc = int(firm.group(3)), int(firm.group(4), 16)
    matching = [int(i) for i, v, _, sz, crc in slots
                if v == "1" and int(sz) == f_size and int(crc, 16) == f_crc]

    if matching:
        assert write not in matching, \
            f"FIRM 과 일치하는 슬롯{matching} 은 백업본이라 보존되어야 하는데 write slot 이 {write}"


def test_boot_log_readable(boot):
    out = boot.cmd("boot log", 2.5)
    assert "boot log :" in out
    m = re.search(r"boot log\s*:\s*(\d+)\s+records", out)
    assert m, f"레코드 수를 못 읽었다:\n{out}"
    if int(m.group(1)) > 0:
        assert "EVENT" in out
