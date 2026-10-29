"""Actual GPIO/ADC driver tests for owner revision CS=PB0,RS=PB1,WR=PB8.
HAL/register mocks verify sequencing and cleanup, not physical touch accuracy.
Imported by run_tests.py so the full suite still uses one command.
"""
from pathlib import Path
import subprocess
import unittest
import struct
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_LR,UC_ARM_REG_SP,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]

def prepare(out):
 global IMAGE
 elf=out/'tft_io.elf'
 subprocess.run(['arm-none-eabi-gcc','-mcpu=cortex-m0plus','-mthumb','-Os','-fno-builtin',
 '-nostdlib','-nostartfiles','-Wall','-Wextra','-Werror','-ITests/io_stubs','-IUser/Inc',
 'User/Src/tft_io.c','Tests/mock_tft_io.c','-TTests/link.ld','-Wl,-e,IO_Init','-lc','-lgcc','-o',str(elf)],cwd=ROOT,check=True)
 binary=elf.with_suffix('.bin');subprocess.run(['arm-none-eabi-objcopy','-O','binary',str(elf),str(binary)],check=True)
 symbols={}
 for line in subprocess.check_output(['arm-none-eabi-nm',str(elf)],text=True).splitlines():
  fields=line.split()
  if len(fields)==3:symbols[fields[2]]=int(fields[0],16)
 IMAGE=binary.read_bytes(),symbols

class IOFirmware:
 def __init__(self):
  binary,self.symbols=IMAGE;self.cpu=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
  self.cpu.mem_map(0x10000,0x30000);self.cpu.mem_map(0x20000000,0x10000);self.cpu.mem_write(0x10000,binary)
  size=self.symbols['_edata']-self.symbols['_sdata']
  if size:self.cpu.mem_write(self.symbols['_sdata'],bytes(self.cpu.mem_read(self.symbols['_sidata'],size)))
  self.call('IO_Init')
 def call(self,name,value=0):
  self.cpu.reg_write(UC_ARM_REG_SP,0x2000fff0);self.cpu.reg_write(UC_ARM_REG_R0,value)
  self.cpu.reg_write(UC_ARM_REG_LR,0x30001)
  self.cpu.emu_start(self.symbols[name]|1,0x30000,count=2000000)
  assert self.cpu.reg_read(UC_ARM_REG_PC)==0x30000,'IO function did not return'
  return self.cpu.reg_read(UC_ARM_REG_R0)
 def read(self,name,n):return bytes(self.cpu.mem_read(self.symbols[name],n))
 def write(self,name,data):self.cpu.mem_write(self.symbols[name],bytes(data))
 def field(self,i):return self.call('IO_Field',i)
 def scan(self):self.call('IO_Scan');return struct.unpack('<HHHBx',self.read('io_raw',8))
 def values(self,values):self.write('io_values',struct.pack('<16I',*values))
 def restored(self,test):
  modes=struct.unpack('<32I',self.read('io_modes',128))
  for i in list(range(8))+[11]:test.assertEqual(modes[i],1)
  for i in (0,1,8,9):test.assertEqual(modes[16+i],1)
  test.assertEqual(self.field(5)&0xe000,0xe000) # NSS/SWD latches preserved.
  test.assertEqual(self.field(5)&0x803,0x803) # RD, XP, YM high.
  test.assertEqual(self.field(6)&0x303,0x303) # CS,RS,WR,RESET high.
  test.assertEqual(self.field(1),0)

class IOTests(unittest.TestCase):
 def test_direct_cs_rs_channels_and_coordinate_pressure_values(self):
  f=IOFirmware();self.assertEqual(f.scan(),(3095,2095,1595,1));self.assertEqual(f.field(0),1)
  self.assertEqual(struct.unpack('<16I',f.read('io_channels',64)),(8,)*4+(9,)*8+(8,)*4)
  self.assertEqual(f.field(3),0);f.restored(self)
 def test_read_touch_does_not_strobe_wr_or_rd(self):
  f=IOFirmware();f.scan();self.assertEqual(f.field(2),16);self.assertEqual(f.field(1),0);self.assertEqual(f.field(3),0)
 def test_all_conversion_setup_failures_restore_lcd(self):
  for i in range(1,17):
   f=IOFirmware();f.call('IO_Failure',0x100+i);f.scan();self.assertEqual(f.field(0),0);f.restored(self)
 def test_all_adc_start_failures_restore_lcd(self):
  for i in range(1,17):
   f=IOFirmware();f.call('IO_Failure',0x200+i);f.scan();self.assertEqual(f.field(0),0);f.restored(self)
 def test_all_adc_poll_timeouts_restore_lcd(self):
  for i in range(1,17):
   f=IOFirmware();f.call('IO_Failure',0x300+i);f.scan();self.assertEqual(f.field(0),0);f.restored(self)
 def test_all_adc_stop_failures_restore_lcd(self):
  for i in range(1,17):
   f=IOFirmware();f.call('IO_Failure',0x400+i);f.scan();self.assertEqual(f.field(0),0);f.restored(self)
 def test_unavailable_adc_leaves_drawing_outputs_usable(self):
  f=IOFirmware();f.call('IO_ADCUnavailable');f.scan();self.assertEqual(f.field(0),0);self.assertEqual(f.field(2),0);f.restored(self)
  f.call('IO_Row');self.assertEqual(f.field(3),13)
 def test_discard_sample_then_median_and_unstable_spread(self):
  f=IOFirmware();values=[4095,1002,1000,1001]+[2000]*4+[500]*4+[3000]*4
  f.values(values);self.assertEqual(f.scan()[0],3094)
  f=IOFirmware();f.values([0,1000,1100,1200]+[2000]*4+[500]*4+[3000]*4)
  self.assertEqual(f.scan()[3],0);self.assertEqual(f.field(0),1);f.restored(self)
 def test_invalid_pressure_order_reports_no_contact_proxy(self):
  f=IOFirmware();f.values([1000]*4+[2000]*4+[3000]*4+[500]*4)
  self.assertEqual(f.scan()[2],0);f.restored(self)
 def test_delay_counter_wrap_and_repeat_scans(self):
  f=IOFirmware();f.call('IO_Time',65525);self.assertEqual(f.scan(),(3095,2095,1595,1));f.restored(self)
  f.call('IO_ResetLog');self.assertEqual(f.scan(),(3095,2095,1595,1));f.restored(self)
 def test_parallel_pixel_writes_use_new_control_mapping(self):
  f=IOFirmware();f.call('IO_Row');self.assertEqual(f.field(3),13)
  self.assertEqual(struct.unpack('<13I',f.read('io_lcd_bytes',52)),(0x2a,0,2,0,2,0x2b,0,3,0,3,0x2c,0x12,0x34))
  self.assertEqual(struct.unpack('<13I',f.read('io_lcd_rs',52)),(0,1,1,1,1,0,1,1,1,1,0,1,1));f.restored(self)
 def test_drawing_after_touch_and_failed_touch(self):
  for fail in (0,0x305):
   f=IOFirmware();f.call('IO_Failure',fail);f.scan();f.call('IO_ResetLog');f.call('IO_Row')
   self.assertEqual(f.field(3),13);f.restored(self)
