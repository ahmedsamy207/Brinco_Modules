"""Run the actual ARM-compiled driver/parser with mocked HAL IO in Unicorn.

Needs Arm GNU tools on PATH and `pip install -r Tests/requirements.txt`.
No probe is used. This verifies software state/protocol behavior, not the
electrical controller, oscillator, HAL registers or real interrupt timing.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_LR, UC_ARM_REG_SP, UC_ARM_REG_PC

ROOT = Path(__file__).resolve().parents[1]


def build(out, polarity):
    """Compile production C with test HAL; keep symbols for emulator calls."""
    elf = out / f"touch_{polarity}.elf"
    subprocess.run([
        "arm-none-eabi-gcc", "-mcpu=cortex-m0plus", "-mthumb", "-Os",
        "-fno-builtin", "-nostartfiles", "-nostdlib", "-Wall", "-Wextra", "-Werror",
        f"-DTOUCH_ACTIVE_HIGH={polarity}", "-ITests/stubs", "-IUser/Inc",
        "User/Src/touch_sensor.c", "User/Src/module_uart.c", "Tests/mock_hal.c",
        "-TTests/link.ld", "-lc", "-lgcc", "-o", str(elf),
    ], cwd=ROOT, check=True)
    binary = elf.with_suffix(".bin")
    subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", str(elf), str(binary)], check=True)
    symbols = {}
    for line in subprocess.check_output(["arm-none-eabi-nm", str(elf)], text=True).splitlines():
        parts = line.split()
        if len(parts) == 3:
            symbols[parts[2]] = int(parts[0], 16)
    return binary.read_bytes(), symbols


class Firmware:
    def __init__(self, image, start=0, raw=0):
        self.binary, self.symbols = image
        self.cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        self.cpu.mem_map(0x10000, 0x30000)
        self.cpu.mem_map(0x20000000, 0x10000)
        self.cpu.mem_write(0x10000, self.binary)
        self.call("Test_Init", start, raw)

    def call(self, function, a=0, b=0):
        self.cpu.reg_write(UC_ARM_REG_SP, 0x2000fff0)
        self.cpu.reg_write(UC_ARM_REG_R0, a & 0xffffffff)
        self.cpu.reg_write(UC_ARM_REG_R1, b & 0xffffffff)
        self.cpu.reg_write(UC_ARM_REG_LR, 0x30001)
        self.cpu.emu_start(self.symbols[function] | 1, 0x30000, count=100000)
        if self.cpu.reg_read(UC_ARM_REG_PC) != 0x30000:
            raise AssertionError("Test function did not return within its instruction budget")
        return self.cpu.reg_read(UC_ARM_REG_R0)

    def step(self, now, raw=0):
        self.call("Test_Step", now, raw)

    def state(self):
        return self.call("Test_State")

    def send(self, data):
        for byte in data:
            self.call("Test_FeedByte", byte)

    def process(self):
        self.call("Test_ProcessUART")

    def output(self):
        size = self.call("Test_TxSize")
        result = bytes(self.cpu.mem_read(self.symbols["test_tx"], size))
        self.call("Test_ClearTx")
        return result

    def command(self, line):
        self.send(line)
        self.process()
        return self.output()


class TouchTests(unittest.TestCase):
    def ready(self, raw=0, polarity=1, start=0):
        fw = Firmware(IMAGES[polarity], start, raw)
        for elapsed in range(1, 611):
            fw.step(start + elapsed, raw)
        return fw

    def test_startup_and_held_touch(self):
        for raw in (0, 1):
            fw = Firmware(IMAGES[1], raw=raw)
            self.assertEqual(fw.command(b"READ\n"), b"ERR NOT_READY\r\n")
            for now in range(1, 610):
                fw.step(now, raw)
                self.assertFalse(fw.state() & 4)
            fw.step(610, raw)
            self.assertEqual(fw.state(), 4 | raw | (raw << 1))

    def test_press_release_and_glitch_rejection(self):
        fw = self.ready()
        for now in range(611, 616):
            fw.step(now, 1)
        fw.step(616, 0)
        self.assertEqual(fw.state(), 4)
        for now in range(617, 627):
            fw.step(now, 1)
            self.assertEqual(fw.state(), 5)
        fw.step(627, 1)
        self.assertEqual(fw.command(b"READ\n"), b"TOUCH 1\r\n")
        for now in range(628, 638):
            fw.step(now, 0)
            self.assertEqual(fw.state(), 6)
        fw.step(638, 0)
        self.assertEqual(fw.command(b"READ\n"), b"TOUCH 0\r\n")

    def test_stale_service_requires_requalification(self):
        fw = self.ready(1)
        fw.call("Test_Time", 631)
        self.assertFalse(fw.state() & 4)
        self.assertEqual(fw.command(b"READ\n"), b"ERR STALE\r\n")
        fw.step(631, 0)
        self.assertFalse(fw.state() & 4)
        for now in range(632, 641):
            fw.step(now, 0)
            self.assertFalse(fw.state() & 4)
        fw.step(641, 0)
        self.assertEqual(fw.state(), 4)

    def test_tick_wrap_and_active_low_configuration(self):
        fw = self.ready(1, start=0xfffffe00)
        self.assertEqual(fw.state(), 7)
        fw = self.ready(0, polarity=0)
        self.assertEqual(fw.command(b"READ\n"), b"TOUCH 1\r\n")
        self.assertEqual(fw.command(b"RAW\n"), b"RAW 0\r\n")

    def test_debounce_and_completed_startup_survive_tick_rollover(self):
        start = 0xfffffd98
        fw = self.ready(start=start)
        for elapsed in range(611, 622):
            fw.step(start + elapsed, 1)
        self.assertEqual(fw.command(b"READ\n"), b"TOUCH 1\r\n")
        # Later wrap with boot_ms == 0 must not re-enable the startup guard.
        fw = self.ready()
        fw.step(0xfffffff0, 1)
        for elapsed in range(1, 33):
            fw.step(0xfffffff0 + elapsed, 1)
        self.assertEqual(fw.command(b"READ\n"), b"TOUCH 1\r\n")

    def test_long_hold_reports_hardware_release_without_software_latch(self):
        fw = self.ready(1)
        for now in range(611, 8611):
            fw.step(now, 1)
        self.assertEqual(fw.command(b"READ\n"), b"TOUCH 1\r\n")
        # Simulate the controller lowering OUT after its own direct-mode limit.
        # This tests the MCU response, not the physical 223's internal timer.
        for now in range(8611, 8622):
            fw.step(now, 0)
        self.assertEqual(fw.command(b"READ\n"), b"TOUCH 0\r\n")

    def test_commands_crlf_partial_lines_and_multiple_lines(self):
        fw = self.ready()
        self.assertEqual(fw.command(b"PING\r\n"), b"PONG\r\n")
        self.assertEqual(fw.command(b"INFO\n"), b"INFO BRINCO TOUCH 1\r\n")
        self.assertEqual(fw.command(b"RAW\n"), b"RAW 0\r\n")
        self.assertEqual(fw.command(b"BAD\n"), b"ERR COMMAND\r\n")
        self.assertEqual(fw.command(b"PI"), b"")
        self.assertEqual(fw.command(b"NG\n"), b"PONG\r\n")
        self.assertEqual(fw.command(b"PING\nREAD\n"), b"PONG\r\n")
        fw.process()
        self.assertEqual(fw.output(), b"TOUCH 0\r\n")

    def test_line_timeout_and_corrupt_line_resynchronization(self):
        fw = self.ready()
        self.assertEqual(fw.command(b"PI"), b"")
        fw.step(1110, 0)
        self.assertEqual(fw.output(), b"ERR LINE_TIMEOUT\r\n")
        self.assertEqual(fw.command(b"NG\n"), b"")
        self.assertEqual(fw.command(b"PING\n"), b"PONG\r\n")
        self.assertEqual(fw.command(b"A" * 32), b"ERR LINE\r\n")
        self.assertEqual(fw.command(b"PING\n"), b"")
        self.assertEqual(fw.command(b"PING\n"), b"PONG\r\n")
        self.assertEqual(fw.command(b"PI\x00NG\n"), b"ERR LINE\r\n")
        fw.process()  # Consume the rejected line's trailing LF.
        self.assertEqual(fw.command(b"PING\n"), b"PONG\r\n")

    def test_overflow_and_uart_error_never_execute_partial_command(self):
        fw = self.ready()
        fw.send(b"A" * 128)
        fw.process()
        self.assertEqual(fw.output(), b"ERR RX\r\n")
        self.assertEqual(fw.command(b"PING\n"), b"")
        self.assertEqual(fw.command(b"PING\n"), b"PONG\r\n")
        fw.send(b"PING")
        fw.call("Test_Error")
        fw.process()
        self.assertEqual(fw.output(), b"ERR RX\r\n")
        self.assertEqual(fw.command(b"\n"), b"")
        self.assertEqual(fw.command(b"PING\n"), b"PONG\r\n")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="brinco-touch-tests-") as directory:
        IMAGES = {polarity: build(Path(directory), polarity) for polarity in (0, 1)}
        unittest.main(verbosity=2)
