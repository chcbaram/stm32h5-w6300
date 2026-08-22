"""리셋 카운트. 물리 버튼은 자동화할 수 없으므로 자동 가능한 부분만 본다."""
import re
from lib import swd


def _count(out):
    m = re.search(r"reset_count\s*:\s*(\d+)", out)
    return int(m.group(1)) if m else None


def test_soft_reset_not_counted(cli):
    """STM32H5 는 소프트 리셋에서도 PINRSTF 가 선다(내부 리셋이 NRST 로 전파).

    SOFT/WDG 를 PIN 보다 먼저 걸러야 resetToBoot() 마다 오진입하지 않는다.
    """
    for _ in range(3):
        cli.ser.reset_input_buffer()
        swd.soft_reset()
        out = cli.read_for(2.5)
        assert _count(out) == 0, f"소프트 리셋은 집계되지 않아야 한다:\n{out[-300:]}"


def test_reset_info_fields(boot):
    out = boot.cmd("reset info", 1.5)
    for k in ("Reset Bits", "Boot Mode", "reset count", "boot try", "fault count", "ecc addr"):
        assert k in out, f"{k} 가 없다"


def test_fault_counter(boot):
    """md 로 없는 주소를 읽어 하드폴트를 낸다. 카운터가 누적되어야 한다."""
    boot.cmd("reset fault clear", 1.0)

    for expect in (1, 2):
        boot.ser.reset_input_buffer()
        boot.ser.write(b"md 0xFFFFFFF0 16\r")
        boot.ser.flush()
        out = boot.read_for(3.0)
        assert "HardFault" in out, "하드폴트가 발생해야 한다"

        boot.ser.write(b"reset boot\r")
        boot.ser.flush()
        boot.read_for(2.5)

        info = boot.cmd("reset info", 1.5)
        m = re.search(r"fault count\s*:\s*(\d+)", info)
        assert m and int(m.group(1)) == expect, f"fault count 가 {expect} 여야 한다:\n{info}"

    boot.cmd("reset fault clear", 1.0)
