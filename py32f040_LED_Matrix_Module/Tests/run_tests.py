"""Run actual ARM-compiled protocol/waveform/transport code with mocked IO.

Requires Arm GNU tools on PATH and pip install -r Tests/requirements.txt.
No probe, real DMA, SPI FIFO/shift-register timing or LED optics are tested.
"""
from pathlib import Path
import binascii
import struct
import subprocess
import tempfile
import unittest
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_LR, UC_ARM_REG_SP, UC_ARM_REG_PC

ROOT=Path(__file__).resolve().parents[1]
FETCH,INFO,STATUS,FRAME,FILL,PIXEL,CLEAR,BRIGHTNESS=0,1,2,16,17,18,19,20

def packet(command, data=b"", sequence=7):
    frame=bytearray(160)
    frame[:6]=bytes([0x42,0x4d,1,sequence,command,len(data)])
    frame[8:8+len(data)]=data
    frame[158:]=binascii.crc_hqx(frame[:158],0xffff).to_bytes(2,"big")
    return frame

def seal(frame):
    frame[158:]=binascii.crc_hqx(frame[:158],0xffff).to_bytes(2,"big")
    return frame

def build(out):
    elf=out/"matrix.elf"
    subprocess.run(["arm-none-eabi-gcc","-mcpu=cortex-m0plus","-mthumb","-Os",
                    "-fno-builtin","-nostdlib","-nostartfiles","-Wall","-Wextra","-Werror",
                    "-ITests/stubs","-IUser/Inc","User/Src/matrix_protocol.c",
                    "User/Src/led_waveform.c","User/Src/spi_transport.c","Host/matrix_host.c","Tests/mock_io.c",
                    "-TTests/link.ld","-lc","-lgcc","-o",str(elf)],cwd=ROOT,check=True)
    binary=elf.with_suffix(".bin")
    subprocess.run(["arm-none-eabi-objcopy","-O","binary",str(elf),str(binary)],check=True)
    symbols={}
    for line in subprocess.check_output(["arm-none-eabi-nm",str(elf)],text=True).splitlines():
        fields=line.split()
        if len(fields)==3: symbols[fields[2]]=int(fields[0],16)
    return binary.read_bytes(),symbols

class Firmware:
    def __init__(self):
        binary,self.symbols=IMAGE
        self.cpu=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
        self.cpu.mem_map(0x10000,0x30000)
        self.cpu.mem_map(0x20000000,0x10000)
        self.cpu.mem_write(0x10000,binary)
        self.call("Test_Init")
    def call(self,name,value=0):
        self.cpu.reg_write(UC_ARM_REG_SP,0x2000fff0)
        self.cpu.reg_write(UC_ARM_REG_R0,value)
        self.cpu.reg_write(UC_ARM_REG_LR,0x30001)
        self.cpu.emu_start(self.symbols[name]|1,0x30000,count=1000000)
        assert self.cpu.reg_read(UC_ARM_REG_PC)==0x30000,"Function did not return"
        return self.cpu.reg_read(UC_ARM_REG_R0)
    def write(self,name,data): self.cpu.mem_write(self.symbols[name],bytes(data))
    def read(self,name,length): return bytes(self.cpu.mem_read(self.symbols[name],length))
    def reply(self):
        self.call("Test_Reply")
        return self.read("test_reply",160)
    def command(self,command,data=b"",sequence=7,frame=None):
        self.write("test_request",packet(command,data,sequence) if frame is None else frame)
        self.call("Test_Command")
        return self.read("test_reply",160)
    def transfer(self,frame):
        self.call("Test_Begin")
        reply=bytes(self.call("Test_Byte",byte) for byte in frame)
        self.call("Test_End")
        return reply

class MatrixTests(unittest.TestCase):
    def test_crc_standard_vector_and_reply_identity(self):
        fw=Firmware()
        fw.write("test_request",b"123456789")
        self.assertEqual(fw.call("Test_CRC"),0x29b1)
        reply=fw.command(INFO)
        self.assertEqual(reply[:8],bytes([0x4d,0x42,1,7,INFO,0,0,4]))
        self.assertEqual(reply[8:12],bytes([48,3,32,1]))
        self.assertEqual(int.from_bytes(reply[158:],"big"),binascii.crc_hqx(reply[:158],0xffff))

    def test_waveform_grb_order_all_pixels_and_reset(self):
        fw=Firmware()
        rgb=bytes((i*37+5)&255 for i in range(144))
        fw.write("test_rgb",rgb);fw.call("Test_Encode",255)
        words=struct.unpack("<1632H",fw.read("test_waveform",3264))
        self.assertEqual(words[:240],(0,)*240)
        self.assertEqual(words[-240:],(0,)*240)
        decoded=[]
        for index in range(240,1392,8):
            value=0
            for duty in words[index:index+8]:
                self.assertIn(duty,(22,68))
                value=(value<<1)|(duty==68)
            decoded.append(value)
        expected=b"".join(bytes([rgb[i+1],rgb[i],rgb[i+2]]) for i in range(0,144,3))
        self.assertEqual(bytes(decoded),expected)
        # Selected values fit both the datasheet table and its narrower note.
        self.assertTrue(.2 <= 22/72 <= .4)
        self.assertTrue(.64 <= 68/72 <= 1.0)
        self.assertTrue(.55 <= (90-22)/72 <= 1.2)
        self.assertTrue(.2 <= (90-68)/72 <= .4)
        self.assertGreaterEqual(240*90/72,100)

    def test_brightness_rounding_and_zero_is_encoded_not_reset(self):
        fw=Firmware();rgb=bytes(range(144));fw.write("test_rgb",rgb)
        for brightness in (0,1,32,128,255):
            fw.call("Test_Encode",brightness)
            words=struct.unpack("<1632H",fw.read("test_waveform",3264))
            decoded=[]
            for i in range(240,1392,8):
                value=0
                for duty in words[i:i+8]: value=(value<<1)|(duty==68)
                decoded.append(value)
            expected=[]
            for i in range(0,144,3):
                expected.extend((rgb[i+channel]*brightness+127)//255 for channel in (1,0,2))
            self.assertEqual(decoded,expected)
            self.assertEqual(fw.read("test_rgb",144),rgb)

    def test_fill_pixel_frame_clear_and_brightness_preserve_rgb(self):
        fw=Firmware()
        self.assertEqual(fw.read("test_pixels",144),bytes(144))
        self.assertEqual(fw.command(FILL,b"\x01\x02\x03")[5],0)
        self.assertEqual(fw.read("test_pixels",144),b"\x01\x02\x03"*48)
        self.assertEqual(fw.command(PIXEL,bytes([47,10,20,30]))[5],0)
        self.assertEqual(fw.read("test_pixels",144)[-3:],bytes([10,20,30]))
        rgb=bytes(range(144))
        self.assertEqual(fw.command(FRAME,rgb)[5],0)
        for level in (0,255,64):
            self.assertEqual(fw.command(BRIGHTNESS,bytes([level]))[5],0)
            self.assertEqual(fw.call("Test_Brightness"),level)
            self.assertEqual(fw.read("test_pixels",144),rgb)
        self.assertEqual(fw.command(CLEAR)[5],0)
        self.assertEqual(fw.read("test_pixels",144),bytes(144))

    def test_corruption_never_changes_pixels(self):
        fw=Firmware();fw.command(FILL,b"\x09\x08\x07")
        before=fw.read("test_pixels",144);shows=fw.call("Test_Shows")
        good=packet(FRAME,bytes(range(144)))
        for index in range(160):
            damaged=good.copy();damaged[index]^=1
            self.assertEqual(fw.command(0,frame=damaged)[5],1)
        self.assertEqual(fw.read("test_pixels",144),before)
        self.assertEqual(fw.call("Test_Shows"),shows)

    def test_format_length_range_and_unknown_commands(self):
        fw=Firmware();shows=fw.call("Test_Shows")
        for command,data,status in [(INFO,b"x",4),(FILL,b"xx",4),(FRAME,b"x",4),
                                    (PIXEL,b"\x30\x01\x02\x03",5),(CLEAR,b"x",4),
                                    (BRIGHTNESS,b"",4),(255,b"",3),(FETCH,b"x",4)]:
            self.assertEqual(fw.command(command,data)[5],status)
        for index in (0,1,2,6,7,152,157):
            damaged=packet(CLEAR);damaged[index]=99
            self.assertEqual(fw.command(0,frame=seal(damaged))[5],2)
        damaged=packet(CLEAR);damaged[5]=145
        self.assertEqual(fw.command(0,frame=seal(damaged))[5],4)
        self.assertEqual(fw.call("Test_Shows"),shows)

    def test_busy_fault_and_start_failure_are_atomic(self):
        fw=Firmware();fw.command(FILL,b"\x01\x02\x03")
        before=fw.read("test_pixels",144);shows=fw.call("Test_Shows")
        fw.call("Test_Busy",1)
        self.assertEqual(fw.command(BRIGHTNESS,b"\xff")[5],6)
        self.assertTrue(fw.reply()[6]&1)
        fw.call("Test_Busy",0);fw.call("Test_Error",1)
        self.assertEqual(fw.command(CLEAR)[5],7)
        self.assertTrue(fw.reply()[6]&2)
        fw.call("Test_Error",0);fw.call("Test_Fail",1)
        self.assertEqual(fw.command(FRAME,bytes(144))[5],7)
        self.assertEqual(fw.read("test_pixels",144),before)
        self.assertEqual(fw.call("Test_Shows"),shows)
        self.assertEqual(fw.command(STATUS)[10],32)

    def test_fetch_preserves_ack_and_reports_live_transport_flags(self):
        fw=Firmware();ack=fw.command(FILL,b"\x01\x02\x03",sequence=55)
        self.assertEqual(fw.command(FETCH,sequence=99),ack)
        fw.call("Test_TransportError")
        reply=fw.command(FETCH)
        self.assertEqual(reply[3:6],ack[3:6]);self.assertTrue(reply[6]&4)
        self.assertFalse(fw.command(STATUS)[6]&4)

    def test_spi_pipelined_ack_and_complete_cs_boundary(self):
        fw=Firmware();frame=packet(FILL,b"\x01\x02\x03",sequence=12)
        fw.call("Test_Begin")
        initial=bytes(fw.call("Test_Byte",byte) for byte in frame)
        self.assertEqual(initial[3:5],b"\xff\xff")
        self.assertEqual(fw.read("test_pixels",144),bytes(144))
        fw.call("Test_End")
        self.assertEqual(fw.read("test_pixels",144),b"\x01\x02\x03"*48)
        ack=fw.transfer(packet(FETCH))
        self.assertEqual(ack[3:6],bytes([12,FILL,0]))
        self.assertEqual(int.from_bytes(ack[158:],"big"),binascii.crc_hqx(ack[:158],0xffff))

    def test_short_overlong_and_unserviced_transactions_rejected(self):
        for size in (0,1,159,161,200):
            fw=Firmware();before=fw.call("Test_Shows")
            fw.transfer((packet(CLEAR)+bytearray(40))[:size])
            self.assertEqual(fw.call("Test_Shows"),before)
            self.assertTrue(fw.reply()[6]&4)
            fw.transfer(packet(FILL,b"\x01\x02\x03"))
            self.assertEqual(fw.call("Test_Shows"),before+1)
        fw=Firmware();before=fw.call("Test_Shows")
        for _ in range(2):
            fw.call("Test_Begin")
            for byte in packet(CLEAR):fw.call("Test_Byte",byte)
            fw.call("Test_EndOnly")
        fw.call("Test_Process")
        self.assertEqual(fw.call("Test_Shows"),before)

    def test_spi_timeout_and_overrun_recover_on_next_boundary(self):
        for fault in ("timeout","overrun"):
            fw=Firmware();before=fw.call("Test_Shows")
            fw.call("Test_Begin")
            for byte in packet(CLEAR):fw.call("Test_Byte",byte)
            if fault=="timeout":
                fw.call("Test_Time",101);fw.call("Test_Process")
            else:fw.call("Test_SPIError")
            fw.call("Test_End")
            self.assertEqual(fw.call("Test_Shows"),before)
            self.assertTrue(fw.reply()[6]&4)
            fw.transfer(packet(CLEAR))
            self.assertEqual(fw.call("Test_Shows"),before+1)

    def test_portable_host_helper_and_corrupted_fetch_retry(self):
        fw=Firmware()
        fw.write("test_request",packet(FILL,b"\x0a\x14\x1e"))
        fw.call("Test_CorruptFetch",1)
        self.assertEqual(fw.call("Test_Host"),1)
        reply=fw.read("test_reply",160)
        self.assertEqual(reply[3:6],bytes([0,FILL,0]))
        self.assertEqual(fw.read("test_pixels",144),b"\x0a\x14\x1e"*48)
        self.assertEqual(fw.call("Test_Shows"),2) # Startup black + one FILL only.
        self.assertEqual(fw.call("Test_Exchanges"),3) # Request, bad FETCH, good FETCH.

    def test_host_timeout_does_not_reexecute_mutation(self):
        fw=Firmware()
        fw.write("test_request",packet(CLEAR))
        fw.call("Test_CorruptFetch",255)
        self.assertEqual(fw.call("Test_Host"),0)
        self.assertEqual(fw.call("Test_Shows"),2)
        self.assertEqual(fw.call("Test_Exchanges"),9)

if __name__=="__main__":
    with tempfile.TemporaryDirectory(prefix="brinco-matrix-tests-") as directory:
        IMAGE=build(Path(directory))
        unittest.main(verbosity=2)
