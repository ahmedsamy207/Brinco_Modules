"""Reference host codec/client for expansion bridge protocol1.

exchange(bytes64)->bytes64 must implement mode0/MSB-first <=300kHz, >=1ms
CS setup and >=5ms CS high. Use one client/outstanding command per chain.
No automatic command retransmission: timeout leaves a mutating outcome unknown.
"""
import binascii
import time

class HubError(RuntimeError):
    """Bridge rejection or unknown result; code4 does not prove non-delivery."""


def packet(command,hop=0,port=0,data=b'',transaction=1):
    """Encode fixed64 bytes. FETCH has every field except signature/version zero."""
    data=bytes(data)
    if not 0<=command<=5 or not 0<=hop<=7 or not 0<=port<=4 or len(data)>48:
        raise ValueError('Invalid command/hop/port/payload size')
    f=bytearray(64);f[:3]=b'BH\x01';f[3]=command
    if command:
        f[4]=hop;f[5]=port;f[6]=len(data)
        f[8:12]=transaction.to_bytes(4,'little');f[14:14+len(data)]=data
    f[62:]=binascii.crc_hqx(f[:62],0xffff).to_bytes(2,'big')
    return f


def decode(frame):
    """Reject corrupted replies before exposing their payload to callers."""
    if len(frame)!=64 or frame[:3]!=b'HB\x01' or frame[6]>48:
        raise ValueError('Invalid bridge reply')
    if int.from_bytes(frame[62:],'big')!=binascii.crc_hqx(frame[:62],0xffff):
        raise ValueError('Reply CRC mismatch')
    if frame[3]>5 or frame[4]>7 or frame[5]>4 or frame[7] or any(frame[12:14]) or any(frame[14+frame[6]:62]):
        raise ValueError('Noncanonical bridge reply')
    return dict(command=frame[3],hop=frame[4],status=frame[5],
                transaction=int.from_bytes(frame[8:12],'little'),data=bytes(frame[14:14+frame[6]]))


class HubClient:
    """Provide an SPI adapter, optionally a monotonic clock/wait for simulation.

    Start IDs at a value newer than previous requests in this powered session.
    Timeout/errors fault the client: reconcile/reset the chain explicitly before
    making another one. Reusing ID1 after reconnect can collide with cached work.
    """
    def __init__(self,exchange,wait=time.sleep,clock=time.monotonic,start_id=1):
        self.exchange=exchange;self.wait=wait;self.clock=clock
        self.next_id=start_id;self.faulted=False

    def command(self,command,hop=0,port=0,data=b''):
        if self.faulted:raise HubError('Previous outcome unresolved; reconcile the chain')
        if command==0:raise ValueError('FETCH is managed internally')
        if not 1<=self.next_id<=0xffffffff:raise OverflowError('Transaction ID exhausted')
        tid=self.next_id;self.next_id+=1
        request=packet(command,hop,port,data,tid)
        deadline=self.clock()+(hop+1)*0.2+0.3
        try:
            self.exchange(request)  # Previous reply; send command exactly once.
            while self.clock()<deadline:
                self.wait(0.005)
                try:r=decode(self.exchange(packet(0)))
                except ValueError:continue
                if (r['transaction'],r['command'],r['hop'])!=(tid,command,hop):continue
                if r['status']==1:continue
                if r['status']:
                    if r['status']==4:self.faulted=True
                    raise HubError(f"Bridge status {r['status']}")
                return r['data']
            self.faulted=True
            raise TimeoutError('Bridge outcome unknown; command was not retransmitted')
        except (OSError,TimeoutError):
            self.faulted=True
            raise
