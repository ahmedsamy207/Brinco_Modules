"""Run actual ARM-compiled renderer/touch/protocol/transport code with mocked IO.

Requires Arm GNU tools on PATH and pip install -r Tests/requirements.txt.
No probe, real GPIO/ADC, panel resistance or SPI FIFO/shift-register timing are tested.
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
FETCH,INFO,STATUS,TOUCH,CLEAR,RECT,PIXELS,ROTATE,LINE,CAL=0,1,2,3,16,17,18,19,20,32

def packet(command, data=b"", sequence=7):
    frame=bytearray(160)
    frame[:6]=bytes([0x42,0x54,1,sequence,command,len(data)])
    frame[8:8+len(data)]=data
    frame[158:]=binascii.crc_hqx(frame[:158],0xffff).to_bytes(2,"big")
    return frame

def seal(frame):
    frame[158:]=binascii.crc_hqx(frame[:158],0xffff).to_bytes(2,"big")
    return frame

def build(out):
    elf=out/"tft.elf"
    subprocess.run(["arm-none-eabi-gcc","-mcpu=cortex-m0plus","-mthumb","-Os",
                    "-fno-builtin","-nostdlib","-nostartfiles","-Wall","-Wextra","-Werror",
                    "-ITests/stubs","-IUser/Inc","User/Src/tft_protocol.c", "User/Src/tft_render.c", "User/Src/tft_touch.c",
                    "User/Src/spi_transport.c","Host/tft_host.c","Tests/mock_io.c",
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
        self.cpu.mem_map(0x20000000,0x80000)
        self.cpu.mem_write(0x10000,binary)
        self.call("Test_Init")
    def call(self,name,value=0):
        self.cpu.reg_write(UC_ARM_REG_SP,0x2007fff0)
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


    def field(self,index):return self.call("Test_Field",index)
    def pixel(self,x,y):
        w=self.field(5)
        return int.from_bytes(self.cpu.mem_read(self.symbols["test_framebuffer"]+(y*w+x)*2,2),"little")
    def raw(self,x,y,z=1000,stable=1):
        self.write("test_raw",struct.pack("<HHHBB",x,y,z,stable,0))


class TFTTests(unittest.TestCase):
    def rect(self,fw,x,y,w,h,color=0xf800):
        return fw.command(RECT,struct.pack("<5H",x,y,w,h,color))
    def cal(self,fw,ends=(100,3900,100,3900),flags=0):
        return fw.command(CAL,struct.pack("<4HB",*ends,flags))
    def contact(self,fw,x=2000,y=2000):
        fw.raw(x,y);fw.call("Test_TouchStep",20);fw.call("Test_TouchStep",40)

    def test_crc_vector_and_reply_identity(self):
        fw=Firmware();fw.write("test_request",b"123456789")
        self.assertEqual(fw.call("Test_CRC"),0x29b1)
        reply=fw.command(INFO)
        self.assertEqual(reply[:6],bytes([0x54,0x42,1,7,INFO,0]))
        self.assertEqual(struct.unpack("<HH",reply[8:12]),(240,320))
        self.assertEqual(int.from_bytes(reply[158:],"big"),binascii.crc_hqx(reply[:158],0xffff))

    def test_full_clear_is_cooperative_and_complete(self):
        fw=Firmware()
        self.assertEqual(fw.command(CLEAR,struct.pack("<H",0x07e0))[5],0)
        self.assertEqual(fw.field(1),0)
        fw.call("Test_DrawStep")
        self.assertEqual((fw.field(1),fw.field(0)),(64,1))
        fw.call("Test_Flush")
        self.assertEqual((fw.field(1),fw.field(0),fw.field(3)),(76800,0,0))
        self.assertEqual((fw.pixel(0,0),fw.pixel(239,319)),(0x07e0,0x07e0))

    def test_rectangle_exact_bounds_and_neighbour_preservation(self):
        fw=Firmware();self.rect(fw,238,318,2,2,0x1234);fw.call("Test_Flush")
        for x,y in ((238,318),(239,318),(238,319),(239,319)):self.assertEqual(fw.pixel(x,y),0x1234)
        self.assertEqual((fw.pixel(237,318),fw.pixel(239,317),fw.field(1)),(0,0,4))

    def test_rgb565_row_chunks_copy_data_after_previous_rectangle(self):
        fw=Firmware();self.rect(fw,0,0,1,5);fw.call("Test_Flush")
        colors=list(range(69))
        data=struct.pack("<3H69H",10,20,69,*colors)
        self.assertEqual(fw.command(PIXELS,data)[5],0)
        fw.write("test_request",bytes(160)) # Job owns a copy, not receive buffer.
        fw.call("Test_DrawStep");self.assertEqual(fw.field(0),1)
        fw.call("Test_DrawStep");self.assertEqual(fw.field(0),0)
        self.assertEqual([fw.pixel(10+i,20) for i in range(69)],colors)
        self.assertEqual(fw.pixel(10,25),0) # No stale rectangle row offset.
        self.assertLessEqual(fw.field(4),64)

    def test_lines_both_directions_and_all_octants(self):
        for a,b in (((10,10),(14,12)),((14,12),(10,10)),((10,10),(12,14)),((12,14),(10,10)),
                    ((14,10),(10,12)),((10,12),(14,10)),((12,10),(10,14)),((10,14),(12,10)),
                    ((0,0),(239,0)),((0,0),(0,319)),((5,5),(5,5))):
            fw=Firmware();self.assertEqual(fw.command(LINE,struct.pack("<5H",*a,*b,0xffff))[5],0)
            fw.call("Test_Flush")
            self.assertEqual((fw.pixel(*a),fw.pixel(*b)),(0xffff,0xffff))
            self.assertEqual(fw.field(1),max(abs(a[0]-b[0]),abs(a[1]-b[1]))+1)
            self.assertEqual(fw.field(3),0)

    def test_rotation_dimensions_and_range_checks(self):
        fw=Firmware()
        for rotation in range(4):
            self.assertEqual(fw.command(ROTATE,bytes([rotation]))[5],0)
            expected=(320,240) if rotation&1 else (240,320)
            self.assertEqual((fw.field(5),fw.field(6)),expected)
            self.assertEqual(self.rect(fw,expected[0]-1,expected[1]-1,1,1)[5],0)
            fw.call("Test_Flush")
            self.assertEqual(self.rect(fw,expected[0],0,1,1)[5],5)
        self.assertEqual(fw.command(ROTATE,b"\x04")[5],5)

    def test_corruption_of_every_packet_byte_cannot_start_drawing(self):
        fw=Firmware();request=packet(RECT,struct.pack("<5H",0,0,10,10,123))
        for i in range(160):
            damaged=request.copy();damaged[i]^=1
            self.assertEqual(fw.command(RECT,frame=damaged)[5],1)
            self.assertEqual((fw.field(0),fw.field(1)),(0,0))

    def test_malformed_lengths_padding_ranges_and_unknown_command(self):
        fw=Firmware()
        for payload in (struct.pack("<5H",0,0,0,1,0),struct.pack("<5H",65535,0,2,1,0),
                        struct.pack("<5H",0,319,1,2,0)):
            self.assertEqual(fw.command(RECT,payload)[5],5)
        for cmd,data in ((CLEAR,b""),(RECT,b"\0"*9),(LINE,b"\0"*11),(PIXELS,b"\0"*7),(CAL,b"\0"*8)):
            self.assertEqual(fw.command(cmd,data)[5],4)
        self.assertEqual(fw.command(PIXELS,struct.pack("<4H",0,0,2,123))[5],4)
        self.assertEqual(fw.command(PIXELS,struct.pack("<4H",0,0,70,123))[5],5)
        for index in (0,1,2,6,7,157):
            request=packet(INFO);request[index]^=2
            self.assertEqual(fw.command(INFO,frame=seal(request))[5],2)
        request=packet(INFO);request[5]=145
        self.assertEqual(fw.command(INFO,frame=seal(request))[5],4)
        self.assertEqual(fw.command(0x7f)[5],3)
        self.assertEqual((fw.field(0),fw.field(1)),(0,0))

    def test_busy_rejects_drawing_but_allows_status_touch_calibration(self):
        fw=Firmware();self.rect(fw,1,1,2,2,7)
        for cmd,data in ((CLEAR,b"\0\0"),(ROTATE,b"\x01"),(RECT,struct.pack("<5H",0,0,1,1,0)),
                         (LINE,struct.pack("<5H",0,0,1,1,0)),(PIXELS,struct.pack("<4H",0,0,1,0))):
            self.assertEqual(fw.command(cmd,data)[5],6)
        for cmd in (INFO,STATUS,TOUCH):self.assertEqual(fw.command(cmd)[5],0)
        self.assertEqual(self.cal(fw)[5],0)
        fw.call("Test_Flush");self.assertEqual((fw.pixel(1,1),fw.field(1),fw.field(7)),(7,4,0))

    def test_fetch_preserves_ack_and_live_flags(self):
        fw=Firmware();self.rect(fw,0,0,10,10)
        reply=fw.command(FETCH,sequence=99)
        self.assertEqual(reply[3:6],bytes([7,RECT,0]));self.assertTrue(reply[6]&1)
        fw.call("Test_Flush");fw.call("Test_TransportError")
        reply=fw.command(FETCH)
        self.assertEqual(reply[3:6],bytes([7,RECT,0]));self.assertEqual(reply[6]&5,4)

    def test_touch_qualification_release_and_uncalibrated_sentinel(self):
        fw=Firmware();fw.raw(2000,2000)
        fw.call("Test_TouchStep",19);self.assertEqual(fw.field(13),0)
        fw.call("Test_TouchStep",20);self.assertEqual((fw.field(8),fw.field(9)),(1,0))
        fw.call("Test_TouchStep",40);self.assertEqual(fw.field(9),1)
        self.assertEqual((fw.field(10),fw.field(11),fw.field(12)),(65535,65535,0))
        fw.raw(2000,2000,z=0);fw.call("Test_TouchStep",60);self.assertEqual(fw.field(9),0)

    def test_pressure_unstable_movement_fault_and_stale_requalification(self):
        fw=Firmware();self.contact(fw)
        for now,z,stable in ((60,79,1),(80,3901,1),(100,1000,0)):
            fw.raw(2000,2000,z,stable);fw.call("Test_TouchStep",now);self.assertEqual(fw.field(9),0)
        fw.raw(2000,2000);fw.call("Test_TouchStep",120);fw.call("Test_TouchStep",140)
        self.assertEqual(fw.field(9),1)
        fw.raw(2300,2000);fw.call("Test_TouchStep",160);self.assertEqual(fw.field(9),0)
        fw.call("Test_TouchFailure",1);fw.call("Test_TouchStep",180)
        self.assertEqual((fw.field(8),fw.field(9)),(0,0))
        fw.call("Test_TouchFailure",0);fw.call("Test_TouchStep",200);fw.call("Test_TouchStep",220)
        fw.call("Test_Time",321);self.assertEqual((fw.field(8),fw.field(9)),(0,0))
        fw.call("Test_TouchStep",400);self.assertEqual(fw.field(9),0)
        fw.call("Test_TouchStep",420);self.assertEqual(fw.field(9),1)

    def test_calibration_endpoints_and_rotation_consistency(self):
        fw=Firmware();self.assertEqual(self.cal(fw)[5],0);self.contact(fw,2000,2000)
        u=1900*239//3800;v=1900*319//3800
        for rotation,xy in enumerate(((u,v),(v,239-u),(239-u,319-v),(319-v,u))):
            fw.command(ROTATE,bytes([rotation]))
            self.assertEqual((fw.field(10),fw.field(11)),xy)
        self.assertEqual(self.cal(fw,(3900,100,3900,100))[5],0)
        fw.command(ROTATE,b"\0")
        self.assertEqual((fw.field(10),fw.field(11)),(u,v))

    def test_calibration_validation_swap_and_clamping(self):
        fw=Firmware();self.cal(fw);self.contact(fw,500,3500)
        before=(fw.field(10),fw.field(11))
        for endpoints,flags in (((0,100,0,1000),0),((0,4096,0,1000),0),((0,4000,0,4000),2)):
            self.assertEqual(self.cal(fw,endpoints,flags)[5],5)
            self.assertEqual((fw.field(10),fw.field(11)),before)
        self.assertEqual(self.cal(fw,flags=1)[5],0)
        self.assertEqual((fw.field(10),fw.field(11)),((3500-100)*239//3800,(500-100)*319//3800))
        self.cal(fw,(1000,3000,1000,3000))
        self.assertEqual((fw.field(10),fw.field(11)),(0,319))

    def test_touch_tick_rollover_and_payload_snapshot(self):
        fw=Firmware();fw.raw(1234,2345)
        fw.call("Test_TouchStep",0xfffffff0);fw.call("Test_TouchStep",4)
        self.assertEqual(fw.field(9),1)
        reply=fw.command(TOUCH)
        self.assertEqual(reply[7],20)
        self.assertEqual(struct.unpack("<3H",reply[14:20]),(1234,2345,1000))
        self.assertEqual(int.from_bytes(reply[24:28],"little"),2)
        fw.call("Test_Time",104);self.assertEqual(fw.field(8),1)
        fw.call("Test_Time",105);self.assertEqual(fw.field(8),0)

    def test_spi_pipeline_short_overlong_and_back_to_back_rejection(self):
        fw=Firmware();old=fw.transfer(packet(INFO,sequence=3))
        self.assertEqual(old[3:5],b"\xff\xff")
        reply=fw.transfer(packet(FETCH))
        self.assertEqual(reply[3:6],bytes([3,INFO,0]))
        for size in (0,1,159,161,200):
            request=packet(CLEAR,b"\0\0")
            fw.call("Test_Begin")
            for byte in (request+bytes(40))[:size]:fw.call("Test_Byte",byte)
            fw.call("Test_End")
            self.assertEqual(fw.field(0),0)
            self.assertTrue(fw.reply()[6]&4)
        # Two CS transactions without foreground re-arm must not reuse old bytes.
        for _ in range(2):
            fw.call("Test_Begin")
            for byte in packet(CLEAR,b"\0\0"):fw.call("Test_Byte",byte)
            fw.call("Test_EndOnly")
        fw.call("Test_Process");self.assertEqual(fw.field(0),0)
        fw.transfer(packet(CLEAR,b"\0\0"));self.assertEqual(fw.field(0),1)

    def test_spi_stuck_cs_and_overrun_recover(self):
        fw=Firmware();fw.call("Test_Begin");fw.call("Test_Time",101);fw.call("Test_Process")
        self.assertTrue(fw.reply()[6]&4);fw.call("Test_End")
        fw.transfer(packet(INFO));reply=fw.transfer(packet(FETCH));self.assertEqual(reply[5],0)
        fw.call("Test_Begin");fw.call("Test_SPIError");fw.call("Test_End")
        self.assertTrue(fw.reply()[6]&4)

    def test_host_helper_retries_only_fetch_and_reports_unknown_outcome(self):
        fw=Firmware();fw.write("test_request",packet(RECT,struct.pack("<5H",0,0,2,2,4)))
        fw.call("Test_CorruptFetch",1)
        self.assertEqual(fw.call("Test_Host"),1);self.assertEqual(fw.call("Test_Exchanges"),3)
        fw.call("Test_Flush");self.assertEqual(fw.field(1),4)
        fw=Firmware();fw.write("test_request",packet(RECT,struct.pack("<5H",0,0,2,2,4)))
        fw.call("Test_CorruptFetch",255)
        self.assertEqual(fw.call("Test_Host"),0);fw.call("Test_Flush");self.assertEqual(fw.field(1),4)


if __name__=="__main__":
    with tempfile.TemporaryDirectory(prefix="brinco-tft-tests-") as directory:
        IMAGE=build(Path(directory))
        unittest.main(verbosity=2)
