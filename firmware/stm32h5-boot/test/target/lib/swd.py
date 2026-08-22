"""STM32_Programmer_CLI 래퍼.

macOS 12 에서는 CLT 1.22 가 실행되지 않는다(Qt 가 macOS 13 이상 요구). 1.21.0 을 쓴다.
"""
import os, re, subprocess

PROG = os.environ.get(
    "STM32_PROG",
    "/opt/ST/STM32CubeCLT_1.21.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI")

ANSI = re.compile(r"\x1b\[[0-9;]*m")


def _run(*args, timeout=90):
    r = subprocess.run([PROG, *args], capture_output=True, text=True, timeout=timeout)
    return ANSI.sub("", r.stdout + r.stderr)


def available():
    try:
        return "STM32CubeProgrammer" in _run("--version", timeout=20)
    except Exception:
        return False


def reset():
    """NRST + 소프트 리셋. 물리 버튼과 달리 SOFT 플래그도 함께 선다."""
    return _run("-c", "port=SWD", "mode=UR", "-rst")


def soft_reset():
    return _run("-c", "port=SWD", "mode=HOTPLUG", "-rst")


def write(path, addr):
    return _run("-c", "port=SWD", "mode=UR", "-w", path, hex(addr), "-v")


def erase_sector(n):
    return _run("-c", "port=SWD", "mode=UR", "-e", str(n))


def read32(addr, count=1):
    out = _run("-c", "port=SWD", "mode=HOTPLUG", "-r32", hex(addr), str(count * 4))
    vals = []
    for line in out.splitlines():
        m = re.match(r"\s*0x[0-9A-Fa-f]{8}\s*:\s*(.+)$", line)
        if m:
            vals += [int(x, 16) for x in m.group(1).split()]
    return vals
