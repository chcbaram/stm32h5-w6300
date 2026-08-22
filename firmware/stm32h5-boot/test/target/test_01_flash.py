"""flash 드라이버. 뱅크2 -> 뱅크1 순서로 확인하고 안전장치를 검증한다."""
import re

SLOT0 = 0x08100000
FIRM  = 0x08020000
BOOT  = 0x08000000


def _read(cli, addr, n=16):
    out = cli.cmd(f"flash read {hex(addr)} {n}", 1.5)
    vals = []
    for line in out.splitlines():
        m = re.match(r"\s*0x[0-9A-Fa-f]{8}\s*:\s*(.+)$", line)
        if m:
            vals += [int(x, 16) for x in m.group(1).split()]
    return vals


def test_info(boot):
    out = boot.cmd("flash info", 1.5)
    assert "swap bank   : 0" in out, "SWAP_BANK 는 0 이어야 한다"
    assert "sector size : 8 KB" in out
    assert f"0x{SLOT0:08X}" in out


def test_bank2_erase_write_read(boot):
    assert "erase OK" in boot.cmd(f"flash erase {hex(SLOT0)} 8192", 2.5)
    assert _read(boot, SLOT0) == [0xFF] * 16, "소거 후 전부 FF"

    assert "write OK" in boot.cmd(f"flash write {hex(SLOT0)} 0x12345678", 1.5)
    assert _read(boot, SLOT0)[:4] == [0x78, 0x56, 0x34, 0x12]


def test_quadword_rewrite_rejected(boot):
    """STM32H5 는 이미 프로그램된 쿼드워드 재기록을 HAL 이 막아주지 않는다.

    ECC 만 조용히 깨지고 나중에 읽을 때 NMI 가 난다. 소프트웨어 가드가 필수다.
    """
    boot.cmd(f"flash erase {hex(SLOT0)} 8192", 2.5)
    boot.cmd(f"flash write {hex(SLOT0)} 0x12345678", 1.5)

    out = boot.cmd(f"flash write {hex(SLOT0)} 0xDEADBEEF", 1.5)
    assert "not blank" in out, "재기록이 거부되어야 한다"
    assert _read(boot, SLOT0)[:4] == [0x78, 0x56, 0x34, 0x12], "원본이 유지되어야"


def test_boot_area_protected(boot):
    assert "protected" in boot.cmd(f"flash erase {hex(BOOT)} 8192", 2.5)
    assert "protected" in boot.cmd(f"flash write {hex(BOOT + 0x10000)} 0x11223344", 1.5)


def test_unaligned_write_rejected(boot):
    assert "align" in boot.cmd(f"flash write {hex(SLOT0 + 4)} 0xAABBCCDD", 1.5)


def test_bank1_firm_area(boot):
    """뱅크1 은 실행 중인 뱅크라 소거/기록 중 AHB 스톨이 걸린다(폴트는 아니다)."""
    assert "erase OK" in boot.cmd(f"flash erase {hex(FIRM)} 8192", 2.5)
    assert "write OK" in boot.cmd(f"flash write {hex(FIRM)} 0xCAFEBABE", 1.5)
    assert _read(boot, FIRM)[:4] == [0xBE, 0xBA, 0xFE, 0xCA]
