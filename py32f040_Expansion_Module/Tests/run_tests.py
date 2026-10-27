"""ARM emulation of actual bridge C; mocks HAL/register timing, not hardware.
Run from project folder: python Tests/run_tests.py
Requires arm-none-eabi-* on PATH and pip install -r Tests/requirements.txt.
"""
from pathlib import Path
import binascii
import subprocess
import tempfile
import unittest
import sys
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_LR,UC_ARM_REG_SP,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Host'))
from expansion_host import packet,decode,HubClient,HubError
FETCH,INFO,WRITE,READ,STATUS,CLEAR=range(6)
def seal(f):
 f[62:]=binascii.crc_hqx(f[:62],0xffff).to_bytes(2,'big');return f

def build(out):
 elf=out/'hub.elf'
 subprocess.run(['arm-none-eabi-gcc','-mcpu=cortex-m0plus','-mthumb','-Os','-fno-builtin',
 '-nostdlib','-nostartfiles','-Wall','-Wextra','-Werror','-ITests/stubs','-IUser/Inc',
 'User/Src/hub.c','User/Src/ports.c','User/Src/downstream.c','User/Src/spi_transport.c',
 'Tests/mock_io.c','-TTests/link.ld','-lc','-lgcc','-o',str(elf)],cwd=ROOT,check=True)
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
        self.cpu.emu_start(self.symbols[name]|1,0x30000,count=1000000)
        assert self.cpu.reg_read(UC_ARM_REG_PC)==0x30000,"Function did not return"
        return self.cpu.reg_read(UC_ARM_REG_R0)
    def write(self,name,data): self.cpu.mem_write(self.symbols[name],bytes(data))
    def read(self,name,length): return bytes(self.cpu.mem_read(self.symbols[name],length))
    def reply(self):
        self.call("Test_Reply")
        return self.read("test_reply",64)
    def command(self,command,data=b"",sequence=7,frame=None):
        self.write("test_request",packet(command,port=1,data=data,transaction=sequence) if frame is None else frame)
        self.call("Test_Command")
        return self.read("test_reply",64)
    def transfer(self,frame):
        self.call("Test_Begin")
        reply=bytes(self.call("Test_Byte",byte) for byte in frame)
        self.call("Test_End")
        return reply



class Chain:
 """Independent CPU/RAM per node; actual downstream bytes clock the next CPU."""
 def __init__(self,n):self.nodes=[Firmware() for _ in range(n)];self.now=0
 def advance(self,ms=5):
  for _ in range(ms):
   self.now+=1
   for i,fw in enumerate(self.nodes):
    fw.call('Test_Time',self.now&0xffffffff);fw.call('Test_Step')
    if fw.call('Test_MasterReady'):
     target=self.nodes[i+1] if i+1<len(self.nodes) else None
     if target:target.call('Test_Begin')
     value=fw.call('Test_MasterFirst')
     for k in range(64):
      rx=target.call('Test_Byte',value) if target else 255
      value=fw.call('Test_MasterByte',rx)
     if target:target.call('Test_End')
   for fw in self.nodes:fw.call('Test_Step')
 def execute(self,frame,ms=200):
  self.nodes[0].transfer(frame);self.advance(ms);return self.nodes[0].reply()

class BridgeTests(unittest.TestCase):
 def cmd(self,f,op,port=0,data=b'',hop=0,tid=1):
  return f.command(op,frame=packet(op,hop,port,data,tid))
 def test_info_crc_and_clock_contract(self):
  f=Firmware();r=self.cmd(f,INFO);self.assertEqual(decode(r)['status'],0)
  self.assertEqual(r[14:19],bytes([1,0,4,7,48]));self.assertEqual(int.from_bytes(r[20:24],'little'),115200)
 def test_all_four_ports_preserve_binary_bytes(self):
  f=Firmware()
  for port in range(1,5):
   data=bytes([0,255,10,13,port]);self.assertEqual(self.cmd(f,WRITE,port,data,tid=port)[5],0)
  f.call('Test_Drain')
  for p in range(4):self.assertEqual(f.read('test_output',8192)[p*2048:p*2048+5],bytes([0,255,10,13,p+1]))
 def test_duplicate_write_does_not_repeat_output(self):
  f=Firmware();frame=packet(WRITE,port=1,data=b'PING\n',transaction=9)
  f.command(WRITE,frame=frame);f.call('Test_Drain');f.command(WRITE,frame=frame);f.call('Test_Drain')
  self.assertEqual(int.from_bytes(f.read('test_output_count',4),'little'),5)
 def test_id_collision_preserves_original_receipt(self):
  f=Firmware();original=self.cmd(f,WRITE,1,b'A',tid=20)
  self.assertEqual(self.cmd(f,WRITE,1,b'B',tid=20),original);f.call('Test_Drain')
  self.assertEqual(int.from_bytes(f.read('test_output_count',4),'little'),1)
 def test_read_is_consumed_once_and_duplicate_is_cached(self):
  f=Firmware()
  for v in [0,255,0x85]:f.call('Test_Feed',v)
  r=self.cmd(f,READ,1,tid=1);self.assertEqual(r[14:17],bytes([0,255,0x85]))
  self.assertEqual(self.cmd(f,READ,1,tid=1),r)
  self.assertEqual(self.cmd(f,READ,1,tid=2)[6],0)
 def test_tx_full_is_atomic_and_recovers(self):
  f=Firmware()
  for tid in range(1,6):self.assertEqual(self.cmd(f,WRITE,2,bytes(48),tid=tid)[5],0)
  self.assertEqual(self.cmd(f,WRITE,2,bytes(17),tid=6)[5],3)
  f.call('Test_Drain');self.assertEqual(int.from_bytes(f.read('test_output_count',8)[4:8],'little'),240)
  self.assertEqual(self.cmd(f,WRITE,2,b'OK',tid=7)[5],0)
 def test_rx_overflow_drops_newest_and_clear_only_clears_flags(self):
  f=Firmware()
  for i in range(270):f.call('Test_Feed',i&255)
  r=self.cmd(f,STATUS,1);self.assertEqual(r[14:16],bytes([0,1]));self.assertEqual(r[18]&1,1)
  self.cmd(f,CLEAR,1,tid=2);r=self.cmd(f,STATUS,1,tid=3)
  self.assertEqual(r[14:16],bytes([0,1]));self.assertEqual(r[18:20],bytes(2))
  self.assertEqual(self.cmd(f,READ,1,tid=4)[14:62],bytes(range(48)))
 def test_ring_indices_wrap_under_sustained_traffic(self):
  f=Firmware()
  for i in range(1400):
   for b in range(48):f.call('Test_Feed',b)
   r=self.cmd(f,READ,1,tid=i+1);self.assertEqual(r[14:62],bytes(range(48)))
 def test_uart_errors_and_tx_start_failure_recover(self):
  f=Firmware();f.call('Test_UARTError',0);f.call('Test_TxFail',1)
  self.cmd(f,WRITE,1,b'A');f.call('Test_Drain');r=self.cmd(f,STATUS,1,tid=2)
  self.assertEqual(r[18]&6,6);self.assertEqual(r[16],1)
  f.call('Test_TxFail',0);f.call('Test_Drain');self.assertEqual(f.read('test_output',1),b'A')
 def test_corruption_at_every_position_cannot_send_uart(self):
  for i in range(64):
   f=Firmware();frame=packet(WRITE,port=1,data=b'RUN',transaction=1);frame[i]^=1
   f.command(WRITE,frame=frame);f.call('Test_Drain');self.assertEqual(f.read('test_output_count',16),bytes(16))
 def test_reserved_length_and_address_validation(self):
  for op,hop,port,data in [(WRITE,0,0,b'A'),(WRITE,0,5,b'A'),(WRITE,8,1,b'A'),(WRITE,0,1,b''),(READ,0,1,b'A'),(INFO,0,1,b''),(9,0,1,b'')]:
   f=Firmware();frame=packet(WRITE,port=1,data=data)
   frame[3]=op;frame[4]=hop;frame[5]=port
   self.assertEqual(f.command(op,frame=seal(frame))[5],2)
  for index in (7,12,13,61):
   f=Firmware();frame=packet(WRITE,port=1,data=b'A');frame[index]=1
   self.assertEqual(f.command(WRITE,frame=seal(frame))[5],2)
 def test_spi_pipelined_reply_and_truncated_overlong_rejection(self):
  f=Firmware();frame=packet(INFO,transaction=44);first=f.transfer(frame)
  self.assertEqual(first[5],1);reply=f.transfer(packet(FETCH));self.assertEqual(reply[8:12],(44).to_bytes(4,'little'))
  for n in (0,1,63,65,80):
   f=Firmware();f.transfer(packet(WRITE,port=1,data=b'A')[:n] if n<64 else packet(WRITE,port=1,data=b'A')+bytes(n-64))
   f.call('Test_Drain');self.assertEqual(f.read('test_output_count',16),bytes(16))
 def test_stuck_cs_and_back_to_back_boundary_recovery(self):
  f=Firmware();f.call('Test_Begin');f.call('Test_Byte',66);f.call('Test_Time',101);f.call('Test_Process');f.call('Test_End')
  f.transfer(packet(INFO,transaction=3));self.assertEqual(f.transfer(packet(FETCH))[5],0)
  f=Firmware();f.call('Test_Begin')
  for b in packet(WRITE,port=1,data=b'A'):f.call('Test_Byte',b)
  f.call('Test_EndOnly');f.call('Test_Begin')
  for b in packet(WRITE,port=1,data=b'B',transaction=2):f.call('Test_Byte',b)
  f.call('Test_End');f.call('Test_Drain');self.assertEqual(f.read('test_output_count',16),bytes(16))
 def test_two_active_hops_and_local_port_isolation(self):
  c=Chain(2);r=c.execute(packet(WRITE,hop=1,port=3,data=b'READ\n',transaction=55))
  self.assertEqual((r[4],r[5]),(1,0));self.assertEqual(r[8:12],(55).to_bytes(4,'little'))
  for f in c.nodes:f.call('Test_Drain')
  self.assertEqual(c.nodes[0].read('test_output_count',16),bytes(16))
  self.assertEqual(c.nodes[1].read('test_output',8192)[4096:4101],b'READ\n')
  c.execute(packet(WRITE,hop=1,port=3,data=b'READ\n',transaction=55))
  c.nodes[1].call('Test_Drain');self.assertEqual(int.from_bytes(c.nodes[1].read('test_output_count',12)[8:12],'little'),5)
 def test_three_hop_binary_read_and_discovery(self):
  c=Chain(3);c.nodes[2].call('Test_Feed',3*256+0x95)
  r=c.execute(packet(READ,hop=2,port=4,transaction=2));self.assertEqual((r[5],r[6],r[14]),(0,1,0x95))
  for hop in range(3):self.assertEqual(c.execute(packet(INFO,hop=hop,transaction=10+hop))[5],0)
 def test_downstream_disconnect_timeout_and_local_recovery(self):
  c=Chain(1);r=c.execute(packet(WRITE,hop=1,port=1,data=b'A',transaction=1),500)
  self.assertEqual(r[5],4);r=c.execute(packet(WRITE,port=1,data=b'B',transaction=2),10)
  self.assertEqual(r[5],0);c.nodes[0].call('Test_Drain');self.assertEqual(c.nodes[0].read('test_output',1),b'B')
 def test_stalled_master_has_bounded_failure_and_can_restart(self):
  f=Firmware();self.cmd(f,INFO,hop=1)
  for t in (5,6,107,108):f.call('Test_Time',t);f.call('Test_Step')
  self.assertEqual(f.reply()[5],4)
  self.assertEqual(self.cmd(f,INFO,tid=2)[5],0)
 def test_async_result_refreshes_already_armed_spi_reply(self):
  c=Chain(2);c.execute(packet(INFO,hop=1,transaction=31),50)
  r=c.nodes[0].transfer(packet(FETCH));self.assertEqual((r[5],r[8]),(0,31))
 def test_host_client_multihop_and_binary_payload(self):
  c=Chain(2)
  def exchange(frame):
   c.advance(5);return c.nodes[0].transfer(frame)
  h=HubClient(exchange,wait=lambda s:c.advance(max(1,int(s*1000))),clock=lambda:c.now/1000)
  self.assertEqual(h.command(WRITE,1,2,b'PING\n'),b'\x05')
  c.nodes[1].call('Test_Feed',256+0)
  self.assertEqual(h.command(READ,1,2),b'\x00')
 def test_pending_command_cannot_be_replaced(self):
  f=Firmware();a=self.cmd(f,INFO,hop=1,tid=1)
  self.assertEqual(self.cmd(f,WRITE,1,b'A',tid=2),a)
  self.assertEqual(self.cmd(f,FETCH),a)

 def test_old_transaction_cannot_replay_after_cache_eviction(self):
  f=Firmware();self.cmd(f,WRITE,1,b'A',tid=10);self.cmd(f,INFO,tid=11)
  self.cmd(f,WRITE,1,b'A',tid=10);f.call('Test_Drain')
  self.assertEqual(int.from_bytes(f.read('test_output_count',4),'little'),1)
 def test_all_eight_supported_hops_route_within_budget(self):
  c=Chain(8);r=c.execute(packet(WRITE,hop=7,port=4,data=b'X',transaction=1),300)
  self.assertEqual(r[5],0);c.nodes[7].call('Test_Drain')
  self.assertEqual(c.nodes[7].read('test_output',8192)[6144:6145],b'X')
 def test_tick_wrap_preserves_forwarding_and_timeout(self):
  c=Chain(2);c.now=0xfffffff0
  for fw in c.nodes:fw.call('Test_Time',c.now)
  r=c.execute(packet(INFO,hop=1,transaction=1),80);self.assertEqual(r[5],0)
 def test_host_faults_after_unknown_outcome_without_resending(self):
  count=[0];now=[0.0]
  def exchange(f):count[0]+=f[3]!=0;return bytes(64)
  def wait(t):now[0]+=t
  h=HubClient(exchange,wait=wait,clock=lambda:now[0])
  with self.assertRaises(TimeoutError):h.command(WRITE,port=1,data=b'A')
  with self.assertRaises(HubError):h.command(WRITE,port=1,data=b'A')
  self.assertEqual(count[0],1)

if __name__=='__main__':
 with tempfile.TemporaryDirectory() as tmp:
  IMAGE=build(Path(tmp));unittest.main()
