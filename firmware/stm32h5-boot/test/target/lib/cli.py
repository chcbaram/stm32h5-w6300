"""부트로더 CLI(시리얼) 헬퍼. 타깃 통합 테스트와 개발 중 수동 확인에 함께 쓴다."""
import glob, subprocess, time
import serial

PROG = "/opt/ST/STM32CubeCLT_1.21.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI"
DEFAULT_PORT = "/dev/cu.usbmodem1412102"   # ST-LINK VCP (SWD 커넥터의 UART)


class Cli:
    def __init__(self, port=DEFAULT_PORT, baud=115200, timeout=0.2):
        self.ser = serial.Serial(port, baud, timeout=timeout)

    def close(self):
        self.ser.close()

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.close()

    def reset(self, wait=1.5):
        """SWD 로 하드 리셋하고 부팅 로그를 돌려준다."""
        self.ser.reset_input_buffer()
        subprocess.run([PROG, "-c", "port=SWD", "mode=UR", "-rst"], capture_output=True)
        return self.read_for(wait)

    def read_for(self, sec):
        buf = b""
        t0 = time.time()
        while time.time() - t0 < sec:
            buf += self.ser.read(4096)
        return buf.decode("utf-8", "replace")

    def cmd(self, line, wait=0.6):
        self.ser.reset_input_buffer()
        self.ser.write((line + "\r").encode())   # cli.c 의 CLI_KEY_ENTER = 0x0D
        self.ser.flush()
        return self.read_for(wait)
