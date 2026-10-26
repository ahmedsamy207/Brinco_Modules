"""Run the actual ARM-compiled statistics/UART parser with mocked HAL IO in Unicorn.

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


def build(out):
    """Compile production C with test HAL; keep symbols for emulator calls."""
    elf = out / "sound.elf"
    subprocess.run([
        "arm-none-eabi-gcc", "-mcpu=cortex-m0plus", "-mthumb", "-Os",
        "-fno-builtin", "-nostartfiles", "-nostdlib", "-Wall", "-Wextra", "-Werror",
        "-ITests/stubs", "-IUser/Inc",
        "User/Src/sound_meter.c", "User/Src/module_uart.c", "Tests/mock_hal.c",
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
    def __init__(self, start=0, raw=1):
        self.binary, self.symbols = IMAGE
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

    def publish(self, values, now=120, raw=1, detected=0):
        import struct
        assert len(values)==80
        self.cpu.mem_write(self.symbols["test_samples"],struct.pack("<80H",*values))
        self.call("Test_Publish",now,raw | (bool(detected)<<1))

    def field(self, index):
        return self.call("Test_Field",index)

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


class SoundTests(unittest.TestCase):
    def ready(self):
        fw = Firmware()
        fw.publish([2000,2100]*40)
        return fw

    def test_startup_guard_requires_full_post_settling_window(self):
        fw=Firmware()
        self.assertEqual(fw.command(b"READ\n"),b"ERR NOT_READY\r\n")
        for now in range(20,120,20):
            fw.publish([2048]*80,now)
            self.assertEqual(fw.field(9),0)
        fw.publish([2048]*80,120)
        self.assertEqual(fw.field(9),1)

    def test_dc_bias_does_not_become_sound_level(self):
        for bias in (0,100,2048,3900,4095):
            fw=Firmware();fw.publish([bias]*80)
            self.assertEqual(fw.field(3),bias)
            self.assertEqual(fw.field(4),0)
            self.assertEqual(fw.field(5),0)

    def test_min_max_mean_last_and_full_scale(self):
        fw=Firmware();fw.publish([0,4095]*40)
        self.assertEqual([fw.field(i) for i in range(6)],[4095,0,4095,2048,4095,1000])
        self.assertEqual(fw.field(8),1)

    def test_relative_level_rounding_across_range(self):
        fw=Firmware()
        for span in (1,2,10,100,500,1024,2048,3000,4095):
            fw.publish([0,span]*40)
            self.assertEqual(fw.field(5),(span*1000+2047)//4095)

    def test_rail_flag_boundaries_and_clear_next_window(self):
        fw=Firmware()
        for sample,clipped in ((5,1),(6,0),(4089,0),(4090,1),(2048,0)):
            fw.publish([sample]+[2048]*79)
            self.assertEqual(fw.field(8),clipped)

    def test_detection_window_flag_is_independent_of_current_d0(self):
        fw=Firmware()
        fw.publish([2000]*80,raw=1,detected=1) # Pulse ended before window end.
        self.assertEqual((fw.field(6),fw.field(7)),(1,1))
        fw.publish([2000]*80,140,raw=1,detected=0)
        self.assertEqual(fw.field(7),0)

    def test_stale_age_boundary_and_next_window_recovery(self):
        fw=self.ready()
        fw.call("Test_Time",220)
        self.assertEqual(fw.field(9),1)
        fw.call("Test_Time",221)
        self.assertEqual(fw.command(b"READ\n"),b"ERR STALE\r\n")
        fw.publish([2000]*80,240)
        self.assertEqual(fw.field(9),1)

    def test_startup_freshness_and_completed_guard_survive_tick_wrap(self):
        start=0xffffffc0
        fw=Firmware(start)
        fw.publish([2000]*80,start+100)
        self.assertEqual(fw.field(9),0)
        fw.publish([2000]*80,start+120)
        self.assertEqual(fw.field(9),1)
        fw.call("Test_Time",start+221)
        self.assertEqual(fw.field(9),0)
        # A later wrap must not restart an already completed settling guard.
        fw.publish([2000]*80,0xfffffff0)
        fw.publish([2000]*80,4)
        self.assertEqual(fw.field(9),1)

    def test_fault_is_sticky_and_invalid_samples_do_not_publish(self):
        fw=self.ready();before=fw.field(12)
        fw.publish([5000]+[2000]*79,140)
        self.assertEqual(fw.field(12),before)
        self.assertEqual((fw.field(9),fw.field(10)),(0,1))
        fw.publish([2000]*80,160)
        self.assertEqual(fw.command(b"READ\n"),b"ERR ACQUISITION\r\n")
        self.assertEqual(fw.field(12),before)
        fw=self.ready();fw.call("Test_Fault")
        self.assertEqual(fw.command(b"RAW\n"),b"ERR ACQUISITION\r\n")

    def test_uart_read_raw_info_status_and_sequence(self):
        fw=self.ready()
        self.assertEqual(fw.command(b"PING\r\n"),b"PONG\r\n")
        self.assertEqual(fw.command(b"INFO\n"),b"INFO BRINCO SOUND 1\r\n")
        self.assertEqual(fw.command(b"READ\n"),b"SOUND 24 0 0\r\n")
        self.assertEqual(fw.command(b"RAW\n"),b"RAW 2100 2000 2100 2050 1\r\n")
        self.assertEqual(fw.command(b"STATUS\n"),b"STATUS 1 0 1 0\r\n")
        fw.publish([2000]*80,140)
        self.assertEqual(fw.field(12),2)
        fw.call("Test_Time",240)
        self.assertEqual(fw.command(b"STATUS\n"),b"STATUS 1 0 2 100\r\n")

    def test_partial_multiple_and_unknown_commands(self):
        fw=self.ready()
        self.assertEqual(fw.command(b"PI"),b"")
        self.assertEqual(fw.command(b"NG\n"),b"PONG\r\n")
        self.assertEqual(fw.command(b"PING\nINFO\n"),b"PONG\r\n")
        fw.process()
        self.assertEqual(fw.output(),b"INFO BRINCO SOUND 1\r\n")
        self.assertEqual(fw.command(b"UNKNOWN\n"),b"ERR COMMAND\r\n")

    def test_line_timeout_malformed_and_oversized_line_recovery(self):
        fw=self.ready();fw.command(b"PI")
        fw.call("Test_Time",620);fw.process()
        self.assertEqual(fw.output(),b"ERR LINE_TIMEOUT\r\n")
        self.assertEqual(fw.command(b"NG\n"),b"")
        self.assertEqual(fw.command(b"PING\n"),b"PONG\r\n")
        self.assertEqual(fw.command(b"PI\x00NG\n"),b"ERR LINE\r\n")
        fw.process()
        self.assertEqual(fw.command(b"PING\n"),b"PONG\r\n")
        self.assertEqual(fw.command(b"A"*32),b"ERR LINE\r\n")
        self.assertEqual(fw.command(b"PING\n"),b"")
        self.assertEqual(fw.command(b"PING\n"),b"PONG\r\n")

    def test_rx_overflow_and_uart_error_reject_partial_commands(self):
        fw=self.ready();fw.send(b"A"*128);fw.process()
        self.assertEqual(fw.output(),b"ERR RX\r\n")
        self.assertEqual(fw.command(b"PING\n"),b"")
        self.assertEqual(fw.command(b"PING\n"),b"PONG\r\n")
        fw.send(b"PING");fw.call("Test_Error");fw.process()
        self.assertEqual(fw.output(),b"ERR RX\r\n")
        self.assertEqual(fw.command(b"\n"),b"")
        self.assertEqual(fw.command(b"PING\n"),b"PONG\r\n")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="brinco-sound-tests-") as directory:
        IMAGE=build(Path(directory))
        unittest.main(verbosity=2)
