"""Run the actual ARM-compiled sensor state machine, bus adapter and UART parser with mocked HAL IO in Unicorn.

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
    elf = out / "color.elf"
    subprocess.run([
        "arm-none-eabi-gcc", "-mcpu=cortex-m0plus", "-mthumb", "-Os",
        "-fno-builtin", "-nostartfiles", "-nostdlib", "-Wall", "-Wextra", "-Werror",
        "-ITests/stubs", "-IUser/Inc",
        "User/Src/color_sensor.c", "User/Src/color_bus.c", "User/Src/module_uart.c", "Tests/mock_hal.c",
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

    def step(self,now):
        self.call("Test_Step",now)

    def set_channels(self,values):
        for i,value in enumerate(values):
            self.call("Test_Channel",i,value)

    def ops(self):
        import struct
        count=self.field(11)
        return struct.unpack(f"<{count}I",self.cpu.mem_read(self.symbols["test_ops"],count*4)) if count else ()

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


class ColorTests(unittest.TestCase):
    def trigger(self,fw,base=0):
        for offset in range(10,15):
            before=fw.field(11)
            fw.step(base+offset)
            self.assertLessEqual(fw.field(11)-before,1)
        return base+14

    def finish(self,fw,triggered=14,it=160):
        ready=triggered+it+it//4+10
        fw.step(ready)
        for offset in range(1,5): fw.step(ready+offset)
        return ready+4

    def ready(self,values=(0x1234,0x5678,0x9abc,0xdef0)):
        fw=Firmware();fw.set_channels(values)
        self.trigger(fw);self.finish(fw)
        return fw

    def test_default_gpio_address_word_format_and_force_config(self):
        fw=Firmware()
        self.assertEqual((fw.field(4),fw.field(5),fw.field(13)),(160,1,1))
        fw.step(9)
        self.assertEqual(fw.field(11),0)
        self.trigger(fw)
        self.assertEqual(fw.ops(),(0x23,0x22,0x80000022,0x26))
        self.assertEqual(fw.field(14),0)
        self.assertEqual(fw.field(15),1)

    def test_no_read_before_exposure_wait_or_partial_publication(self):
        fw=Firmware();fw.set_channels((10,20,30,40));self.trigger(fw)
        fw.step(223)
        self.assertEqual(fw.field(11),4)
        fw.step(224)
        for now in range(225,228):
            fw.step(now)
            self.assertEqual(fw.field(6),0)
            self.assertEqual([fw.field(i) for i in range(4)],[0]*4)
        fw.step(228)
        self.assertEqual([fw.field(i) for i in range(4)],[10,20,30,40])
        self.assertEqual((fw.field(6),fw.field(9),fw.field(10)),(1,228,1))

    def test_individual_register_order_little_endian_and_next_trigger(self):
        fw=self.ready()
        self.assertEqual([fw.field(i) for i in range(4)],[0x1234,0x5678,0x9abc,0xdef0])
        self.assertEqual([op>>16 & 0xff for op in fw.ops()[4:]],[8,9,10,11])
        self.assertEqual(fw.field(15),1)
        fw.step(229)
        self.assertEqual(fw.field(15),2)
        self.assertEqual(fw.field(14),0)

    def test_all_integration_settings_mapping_and_wait(self):
        for code,it in enumerate((40,80,160,320,640,1280)):
            fw=Firmware();fw.set_channels((1,2,3,4))
            self.assertEqual(fw.call("Test_SetIT",it),1)
            self.trigger(fw)
            self.assertEqual(fw.ops()[0],(code<<4)|3)
            self.assertEqual(fw.ops()[3],(code<<4)|6)
            fw.step(14+it+it//4+9)
            self.assertEqual(fw.field(11),4)
            self.finish(fw,it=it)
            self.assertEqual(fw.field(6),1)

    def test_invalid_and_unchanged_settings_preserve_current_reading(self):
        fw=self.ready();ops=fw.field(11)
        for it in (0,39,41,100,65535): self.assertEqual(fw.call("Test_SetIT",it),0)
        self.assertEqual(fw.call("Test_SetLED",2),0)
        self.assertEqual(fw.call("Test_SetIT",160),1)
        self.assertEqual(fw.call("Test_SetLED",1),1)
        self.assertEqual((fw.field(6),fw.field(11)),(1,ops))

    def test_setting_change_discards_inflight_exposure_and_settles(self):
        fw=Firmware();self.trigger(fw)
        fw.call("Test_Time",100)
        self.assertEqual(fw.call("Test_SetLED",0),1)
        self.assertEqual((fw.field(5),fw.field(13),fw.field(6)),(0,0,0))
        fw.step(109);self.assertEqual(fw.field(11),4)
        self.trigger(fw,100);self.finish(fw,114)
        self.assertEqual(fw.field(10),1) # Original exposure was never published.
        fw.call("Test_SetIT",80)
        self.assertEqual((fw.field(4),fw.field(6)),(80,0))

    def test_failure_at_each_transaction_never_publishes_partial_sample(self):
        for fail in range(1,9):
            fw=Firmware();fw.set_channels((111,222,333,444));fw.call("Test_FailAt",fail)
            self.trigger(fw);self.finish(fw)
            self.assertEqual((fw.field(6),fw.field(7),fw.field(10)),(0,1,0))
            self.assertEqual([fw.field(i) for i in range(4)],[0]*4)
            self.assertEqual(fw.command(b"READ\n"),b"ERR SENSOR\r\n")

    def test_read_failure_invalidates_previous_sample_and_retry_recovers(self):
        fw=self.ready();fw.set_channels((1,2,3,4))
        fw.step(229);fw.call("Test_FailAt",fw.field(11)+2) # Fail second colour read.
        self.finish(fw,229)
        self.assertEqual((fw.field(6),fw.field(7),fw.field(10)),(0,1,1))
        self.assertEqual([fw.field(i) for i in range(4)],[0x1234,0x5678,0x9abc,0xdef0])
        ops=fw.field(11);fw.step(940) # Error at 441; 499 ms is too soon.
        self.assertEqual(fw.field(11),ops)
        fw.step(941);self.trigger(fw,941);self.finish(fw,955)
        self.assertEqual((fw.field(6),fw.field(7),fw.field(10)),(1,0,2))
        self.assertEqual([fw.field(i) for i in range(4)],[1,2,3,4])

    def test_configuration_mismatch_and_bus_init_failure_report_error(self):
        for mode in ("Test_BadConf","Test_InitFailure"):
            fw=Firmware();fw.call(mode,1)
            self.trigger(fw)
            self.assertEqual((fw.field(6),fw.field(7),fw.field(15)),(0,1,0))

    def test_saturation_mask_boundaries_and_dark_validity(self):
        for mask in range(16):
            values=[65520 if mask & (1<<i) else 65519 for i in range(4)]
            fw=self.ready(values)
            self.assertEqual(fw.field(8),mask)
        fw=self.ready((0,0,0,0))
        self.assertEqual((fw.field(6),fw.field(8)),(1,0))

    def test_stale_reading_and_tick_wrap(self):
        fw=self.ready();fw.call("Test_Time",538)
        self.assertEqual(fw.field(6),1)
        fw.call("Test_Time",539)
        self.assertEqual(fw.field(6),0)
        start=0xffffff80;fw=Firmware(start);fw.set_channels((1,2,3,4))
        self.trigger(fw,start);self.finish(fw,start+14)
        self.assertEqual((fw.field(6),fw.field(10)),(1,1))

    def test_uart_identity_read_alias_status_and_setting_ack(self):
        fw=self.ready((100,200,300,400))
        self.assertEqual(fw.command(b"PING\r\n"),b"PONG\r\n")
        self.assertEqual(fw.command(b"INFO\n"),b"INFO BRINCO COLOR 1\r\n")
        for cmd in (b"READ\n",b"RAW\n"):
            self.assertEqual(fw.command(cmd),b"COLOR 100 200 300 400 0\r\n")
        self.assertEqual(fw.command(b"STATUS\n"),b"STATUS 1 0 160 1 1 0 0\r\n")
        self.assertEqual(fw.command(b"LED 0\n"),b"OK LED 0\r\n")
        self.assertEqual(fw.command(b"READ\n"),b"ERR NOT_READY\r\n")
        self.assertEqual(fw.command(b"IT 1280\n"),b"OK IT 1280\r\n")
        self.assertEqual((fw.field(4),fw.field(5)),(1280,0))

    def test_uart_argument_validation_does_not_change_settings(self):
        fw=self.ready()
        for cmd in (b"LED",b"LED 2",b"LED -1",b"LED 1 ",b"IT",b"IT 100",b"IT 040",b"IT +40",b"IT 40 ",b"IT  40"):
            self.assertEqual(fw.command(cmd+b"\n"),b"ERR ARGUMENT\r\n")
            self.assertEqual((fw.field(4),fw.field(5),fw.field(6)),(160,1,1))
        self.assertEqual(fw.command(b"UNKNOWN\n"),b"ERR COMMAND\r\n")

    def test_partial_multiple_and_malformed_line_recovery(self):
        fw=self.ready()
        self.assertEqual(fw.command(b"PI"),b"")
        self.assertEqual(fw.command(b"NG\n"),b"PONG\r\n")
        self.assertEqual(fw.command(b"PING\nINFO\n"),b"PONG\r\n")
        fw.process();self.assertEqual(fw.output(),b"INFO BRINCO COLOR 1\r\n")
        self.assertEqual(fw.command(b"LED \x001\n"),b"ERR LINE\r\n")
        fw.process();self.assertEqual(fw.command(b"PING\n"),b"PONG\r\n")
        self.assertEqual(fw.command(b"A"*32),b"ERR LINE\r\n")
        self.assertEqual(fw.command(b"PING\n"),b"")
        self.assertEqual(fw.command(b"PING\n"),b"PONG\r\n")

    def test_line_timeout_overflow_and_uart_errors_discard_setting_suffix(self):
        fw=self.ready();fw.command(b"LED ")
        fw.call("Test_Time",728);fw.process()
        self.assertEqual(fw.output(),b"ERR LINE_TIMEOUT\r\n")
        self.assertEqual(fw.command(b"0\n"),b"")
        self.assertEqual(fw.field(5),1)
        fw.send(b"A"*128);fw.process();self.assertEqual(fw.output(),b"ERR RX\r\n")
        self.assertEqual(fw.command(b"LED 0\n"),b"")
        self.assertEqual(fw.field(5),1)
        fw.send(b"LED 0");fw.call("Test_Error");fw.process()
        self.assertEqual(fw.output(),b"ERR RX\r\n")
        self.assertEqual(fw.command(b"\n"),b"")
        self.assertEqual(fw.field(5),1)
        self.assertEqual(fw.command(b"PING\n"),b"PONG\r\n")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="brinco-color-tests-") as directory:
        IMAGE=build(Path(directory))
        unittest.main(verbosity=2)
