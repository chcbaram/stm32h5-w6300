import os, subprocess, sys, time
import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib.cli import Cli
from lib import swd
from lib.cmdproto import (CmdChannel, SerialTransport, HidTransport,
                          BOOT_CMD_INFO, parse_info, DEV_MODE_BOOT)

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
APP  = os.path.normpath(os.path.join(ROOT, "..", "stm32h5-fw"))


def pytest_addoption(parser):
    parser.addoption("--slow", action="store_true", help="느린 시험(폴트 복구 등)도 돌린다")


@pytest.fixture(scope="session", autouse=True)
def require_hardware():
    if not swd.available():
        pytest.skip("ST-LINK 를 찾지 못했다", allow_module_level=True)


@pytest.fixture(scope="session")
def cli():
    """ST-LINK VCP 의 부트로더/앱 CLI."""
    with Cli() as c:
        yield c


@pytest.fixture
def boot(cli):
    """부트로더에 머무는 상태로 만든다.

    앱이 돌고 있으면 reset boot 로 재진입시킨다. 물리 버튼을 누를 수 없으므로
    더블클릭 대신 이 경로를 쓴다(같은 판정 분기로 들어간다).
    """
    cli.reset(2.5)
    cli.ser.write(b"reset boot\r")
    cli.ser.flush()
    out = cli.read_for(3.0)
    if "Boot Mode" not in out:
        out += cli.read_for(2.0)
    assert "Boot Mode" in out, f"부트 모드 진입 실패:\n{out[-400:]}"
    time.sleep(2.0)          # USB 열거 대기
    return cli


@pytest.fixture
def hid(boot):
    """부트로더의 HID 채널."""
    try:
        t = HidTransport(0xCAFE, 0xB003)
    except Exception as e:
        pytest.skip(f"HID 장치를 열 수 없다: {e}")
    ch = CmdChannel(t)
    yield ch
    t.close()


@pytest.fixture
def app_uf2():
    """앱 빌드 산출물 .uf2 경로."""
    import glob
    f = sorted(glob.glob(os.path.join(APP, "build", "*.uf2")))
    if not f:
        pytest.skip("앱 .uf2 가 없다. 먼저 앱을 빌드한다.")
    return f[-1]


@pytest.fixture
def app_bin():
    p = os.path.join(APP, "build", "stm32h5-w6300-fw.bin")
    if not os.path.exists(p):
        pytest.skip("앱 .bin 이 없다. 먼저 앱을 빌드한다.")
    return p
