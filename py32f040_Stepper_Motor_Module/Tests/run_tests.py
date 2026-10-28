"""Execute real ARM motion, protocol, UART and GPIO/timer drivers in Unicorn.
Mock HAL/register semantics exclude electrical, torque, timing and thermal proof.
Run: python Tests/run_tests.py (Arm GNU tools and requirements.txt required).
"""
from pathlib import Path
import subprocess
import tempfile
import unittest
import struct
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_LR,UC_ARM_REG_SP,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]
IDLE,WAKING,MOVING,RUNNING,STOPPING,HOLD,FAULT=range(7)
def build(out):
 elf=out/'stepper.elf'
 subprocess.run(['arm-none-eabi-gcc','-mcpu=cortex-m0plus','-mthumb','-Os','-fno-builtin',
 '-nostdlib','-nostartfiles','-Wall','-Wextra','-Werror','-ITests/stubs','-IUser/Inc',
 'User/Src/motion.c','User/Src/motion_protocol.c','User/Src/motor_io.c','User/Src/module_uart.c',
 'Tests/mock_hal.c','-TTests/link.ld','-lc','-lgcc','-o',str(elf)],cwd=ROOT,check=True)
 binary=elf.with_suffix('.bin');subprocess.run(['arm-none-eabi-objcopy','-O','binary',str(elf),str(binary)],check=True)
 symbols={}
 for line in subprocess.check_output(['arm-none-eabi-nm',str(elf)],text=True).splitlines():
  fields=line.split()
  if len(fields)==3:symbols[fields[2]]=int(fields[0],16)
 return binary.read_bytes(),symbols

class Firmware:
    def __init__(self):
        binary,self.symbols=IMAGE
        self.cpu=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
        self.cpu.mem_map(0x10000,0x30000)
        self.cpu.mem_map(0x20000000,0x80000)
        self.cpu.mem_write(0x10000,binary)
        size=self.symbols['_edata']-self.symbols['_sdata']
        if size:
            self.cpu.mem_write(self.symbols['_sdata'],bytes(self.cpu.mem_read(self.symbols['_sidata'],size)))
        self.call("Test_Init")
    def call(self,name,value=0):
        self.cpu.reg_write(UC_ARM_REG_SP,0x2007fff0)
        self.cpu.reg_write(UC_ARM_REG_R0,value)
        self.cpu.reg_write(UC_ARM_REG_LR,0x30001)
        self.cpu.emu_start(self.symbols[name]|1,0x30000,count=2000000)
        assert self.cpu.reg_read(UC_ARM_REG_PC)==0x30000,"Function did not return"
        return self.cpu.reg_read(UC_ARM_REG_R0)
    def write(self,name,data): self.cpu.mem_write(self.symbols[name],bytes(data))
    def read(self,name,length): return bytes(self.cpu.mem_read(self.symbols[name],length))

    def field(self,i):return self.call('Test_Field',i)
    def position(self):
        v=self.field(1);return v if v<0x80000000 else v-0x100000000
    def send(self,line):
        for b in line:self.call('Test_FeedByte',b)
    def output(self):
        n=self.call('Test_TxSize');data=self.read('test_tx',n);self.call('Test_ClearTx');return data
    def command(self,line):
        self.send(line if isinstance(line,bytes) else (line+'\n').encode());self.call('Test_ProcessUART');return self.output()
    def ms(self,n,keepalive=True):
        for elapsed in range(0,n,50):
            if keepalive:self.call('Motion_KeepAlive')
            self.call('Test_Ticks',min(50,n-elapsed)*20)
    def finish(self,limit_ms=30000):
        for i in range(limit_ms//50):
            if self.field(0) in (HOLD,IDLE,FAULT):return
            self.ms(50)
        raise AssertionError('Move failed to terminate')
    def trace(self):
        n=int.from_bytes(self.read('test_trace_count',4),'little')
        return struct.unpack('<'+'I'*n,self.read('test_trace',4*n))

class StepperTests(unittest.TestCase):
 def test_boot_released_with_expected_defaults(self):
  f=Firmware();self.assertEqual(f.field(0),IDLE);self.assertEqual(f.call('Test_Output')&0x73,0)
  self.assertEqual((f.field(4),f.field(5)),(50,100));self.assertEqual(f.command('PING'),b'PONG\r\n')
  self.assertIn(b'NFP-15BY-A FULL 20',f.command('INFO'))
 def test_wake_delay_precedes_first_coil_drive(self):
  f=Firmware();self.assertEqual(f.command('MOVE 1'),b'OK\r\n')
  self.assertEqual(f.call('Test_Output')&0x73,0x40)
  f.call('Test_Ticks',39);self.assertEqual(f.field(0),WAKING);self.assertEqual(f.call('Test_Output')&0x33,0)
  f.call('Test_Ticks',1);self.assertEqual(f.field(0),MOVING);self.assertEqual(f.call('Test_Output')&0x33,0x11)
 def test_exact_positive_revolution_and_legal_two_coil_phases(self):
  f=Firmware();f.command('MOVE 20');f.finish()
  self.assertEqual((f.position(),f.field(2),f.field(0)),(20,0,HOLD))
  trace=f.trace();self.assertEqual(len(trace),21)
  self.assertEqual([v&0x33 for v in trace[:5]],[0x11,0x21,0x22,0x12,0x11])
  self.assertTrue(all(v&0x33 in (0x11,0x21,0x22,0x12) for v in trace))
 def test_negative_move_and_reverse_from_hold(self):
  f=Firmware();f.command('MOVE -20');f.finish();self.assertEqual(f.position(),-20)
  self.assertEqual([v&0x33 for v in f.trace()[:5]],[0x11,0x12,0x22,0x21,0x11])
  f.command('MOVE 7');self.assertEqual(f.field(0),MOVING);f.finish();self.assertEqual(f.position(),-13)
 def test_single_step_and_low_speed_complete_without_extra_steps(self):
  for count in (1,-1,2,-3):
   f=Firmware();f.command('SPEED 1');f.command(f'MOVE {count}');f.finish()
   self.assertEqual(f.position(),count);self.assertEqual(len(f.trace()),abs(count)+1)
 def test_continuous_run_and_acceleration_then_speed_change(self):
  f=Firmware();f.command('SPEED 100');f.command('ACCEL 200');f.command('RUN 1');f.ms(250)
  self.assertGreater(f.field(3),45*65536);self.assertLess(f.field(3),51*65536)
  before=f.position();f.ms(1000);self.assertGreater(f.position(),before)
  self.assertEqual(f.field(3),100*65536)
  f.command('SPEED 20');f.ms(500);self.assertEqual(f.field(3),20*65536);self.assertEqual(f.field(0),RUNNING)
 def test_decelerated_stop_holds_then_release_disables(self):
  f=Firmware();f.command('SPEED 100');f.command('ACCEL 200');f.command('RUN -1');f.ms(700)
  before=f.position();self.assertEqual(f.command('STOP'),b'OK\r\n');self.assertEqual(f.field(0),STOPPING)
  f.ms(100);self.assertGreater(f.field(3),0);self.assertLess(f.position(),before)
  f.finish();self.assertEqual(f.field(0),HOLD);self.assertNotEqual(f.call('Test_Output')&0x33,0)
  f.command('RELEASE');self.assertEqual(f.call('Test_Output')&0x73,0);self.assertEqual(f.field(0),IDLE)
 def test_release_cancels_move_immediately(self):
  f=Firmware();f.command('MOVE 100');f.ms(100);f.command('RELEASE');p=f.position();f.ms(500)
  self.assertEqual(f.position(),p);self.assertEqual((f.field(0),f.field(2)),(IDLE,0));self.assertEqual(f.call('Test_Output')&0x73,0)
 def test_stop_during_wake_never_energizes(self):
  f=Firmware();f.command('RUN 1');f.command('STOP');f.ms(100)
  self.assertEqual(f.field(0),IDLE);self.assertEqual(f.trace(),())
 def test_busy_commands_and_zero_do_not_replace_motion(self):
  f=Firmware();f.command('MOVE 20');self.assertEqual(f.command('MOVE -20'),b'ERR BUSY\r\n')
  self.assertEqual(f.command('RUN -1'),b'ERR BUSY\r\n');self.assertEqual(f.command('ZERO'),b'ERR BUSY\r\n')
  f.finish();self.assertEqual(f.position(),20);self.assertEqual(f.command('ZERO'),b'OK\r\n');self.assertEqual(f.position(),0)
 def test_driver_fault_irq_latches_even_after_pin_recovers(self):
  f=Firmware();f.command('RUN 1');f.ms(100);f.call('Test_FaultPin',1);f.call('Test_FaultIRQ')
  self.assertEqual((f.field(0),f.field(6)),(FAULT,1));self.assertEqual(f.call('Test_Output')&0x73,0)
  self.assertEqual(f.command('CLEAR'),b'ERR FAULT\r\n');f.call('Test_FaultPin',0)
  self.assertEqual(f.command('RUN 1'),b'ERR FAULT\r\n');self.assertEqual(f.command('CLEAR'),b'OK\r\n')
  self.assertEqual(f.field(0),IDLE);self.assertEqual(f.command('RUN 1'),b'OK\r\n')
 def test_polling_catches_fault_if_exti_is_missed(self):
  f=Firmware();f.command('RUN 1');f.ms(50);f.call('Test_FaultPin',1);f.call('Test_Ticks',1)
  self.assertEqual(f.field(6),1);self.assertEqual(f.call('Test_Output')&0x73,0)
 def test_fault_already_low_rejects_motion(self):
  f=Firmware();f.call('Test_FaultPin',1);self.assertEqual(f.command('MOVE 1'),b'ERR FAULT\r\n');self.assertEqual(f.field(0),FAULT)
 def test_host_timeout_releases_both_motion_and_hold(self):
  for command in ('RUN 1','MOVE 1'):
   f=Firmware();f.command(command);f.ms(2000,keepalive=False)
   self.assertEqual((f.field(0),f.field(6)),(FAULT,2));self.assertEqual(f.call('Test_Output')&0x73,0)
 def test_status_refreshes_lease_and_reports_units(self):
  f=Firmware();f.command('RUN 1')
  for _ in range(5):
   f.ms(1000,keepalive=False);r=f.command('STATUS');self.assertIn(b'STATUS RUNNING POS',r)
  self.assertEqual(f.field(6),0);self.assertIn(b'VEL 50000 SPEED 50 ACCEL 100 FAULT 0',r)
 def test_invalid_commands_cannot_renew_lease(self):
  f=Firmware();f.command('RUN 1')
  for _ in range(4):f.ms(500,keepalive=False);f.command('NONSENSE')
  self.assertEqual(f.field(6),2)
 def test_strict_numeric_bounds_and_no_motion_from_malformed_input(self):
  f=Firmware()
  for cmd in ('MOVE 0','MOVE 1000001','MOVE -2147483648','MOVE 1x','MOVE  1','RUN 0','RUN 2',
              'SPEED 0','SPEED -1','SPEED 501','ACCEL 0','ACCEL 5001','MOVE 1 2','MOVE +','MOVE 4294967296'):
   self.assertEqual(f.command(cmd),b'ERR ARG\r\n',cmd);self.assertEqual(f.field(0),IDLE)
  self.assertEqual(f.command('move 1'),b'ERR COMMAND\r\n')
 def test_crlf_valid_but_embedded_cr_or_nul_cannot_form_motion(self):
  f=Firmware();self.assertEqual(f.command(b'PING\r\n'),b'PONG\r\n')
  for data in (b'RU\rN 1\n',b'RUN \x001\n'):
   f.send(data);f.call('Test_ProcessUART');f.call('Test_ProcessUART');self.assertEqual(f.field(0),IDLE);f.output()
  self.assertEqual(f.command('MOVE 1'),b'OK\r\n')
 def test_line_timeout_discards_suffix_until_delimiter(self):
  f=Firmware();f.send(b'MO');f.call('Test_ProcessUART');f.call('Test_Time',501);f.call('Test_ProcessUART')
  self.assertEqual(f.output(),b'ERR LINE_TIMEOUT\r\n');f.send(b'VE 20\n');f.call('Test_ProcessUART')
  self.assertEqual(f.field(0),IDLE);self.assertEqual(f.command('MOVE 1'),b'OK\r\n')
 def test_rx_overflow_and_uart_error_latch_release(self):
  for overflow in (True,False):
   f=Firmware();f.command('RUN 1');f.ms(100)
   if overflow:f.send(b'X'*200)
   else:f.call('Test_Error')
   f.call('Test_ProcessUART');self.assertIn(b'ERR RX',f.output());self.assertEqual(f.field(6),4)
   self.assertEqual(f.call('Test_Output')&0x73,0)
   f.command(b'\n');f.command('CLEAR');self.assertEqual(f.command('MOVE 1'),b'OK\r\n')
 def test_line_overlength_discards_dangerous_suffix(self):
  f=Firmware();f.send(b'X'*70+b'RUN 1\n');f.call('Test_ProcessUART');f.call('Test_ProcessUART')
  self.assertEqual(f.field(0),IDLE);self.assertEqual(f.command('PING'),b'ERR LINE\r\nPONG\r\n')
 def test_counter_limits_fault_before_unrepresentable_step(self):
  for position,direction in ((2147483647,1),(-2147483648,-1)):
   f=Firmware();f.cpu.mem_write(f.symbols['motor'],struct.pack('<i',position));f.command(f'RUN {direction}');f.ms(1000)
   self.assertEqual(f.field(6),3);self.assertEqual(f.position(),position);self.assertEqual(f.call('Test_Output')&0x73,0)
 def test_finite_profile_slows_toward_endpoint(self):
  f=Firmware();f.command('SPEED 200');f.command('ACCEL 500');f.command('MOVE 100')
  maximum=0;late=0
  for _ in range(500):
   f.ms(10);maximum=max(maximum,f.field(3))
   if 0<f.field(2)<5:late=f.field(3)
   if f.field(0)==HOLD:break
  self.assertEqual(f.position(),100);self.assertGreater(maximum,late);self.assertGreater(maximum,100*65536)
 def test_pause_in_uart_processing_does_not_pause_timer_motion(self):
  f=Firmware();f.command('RUN 1');f.ms(300);p=f.position()
  f.send(b'PING\n');f.ms(100,keepalive=False);self.assertGreater(f.position(),p)
  f.call('Test_ProcessUART');self.assertEqual(f.output(),b'PONG\r\n')

if __name__=='__main__':
 with tempfile.TemporaryDirectory() as tmp:
  IMAGE=build(Path(tmp));unittest.main()
