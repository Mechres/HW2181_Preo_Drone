#!/usr/bin/env python3
"""Quick live snapshot of two RAM/flash locations relevant to checking
whether the drone has received/accepted a radio packet -- more reliable
than breakpointing "packet received" functions (see SWD_DEBUGGING_GUIDE.md
for why). Requires `openocd -f ../openocd_hw2181.cfg` already running.

- 0x200000E8 (RAM): raw incoming RF packet buffer per RADIO_PACKET_SPEC.md
  (unverified address -- wasn't independently confirmed against
  disassembly, treat as a starting point not ground truth).
- 0x8FF8 (flash): persistent pairing signature (VERIFIED, see
  SESSION2_VERIFIED_FINDINGS.md) -- only changes on genuine successful
  pairing, so a before/after diff across a test transmission is a
  trustworthy signal.

For checking whether a SPECIFIC stick value made it through, see the
scaled-value RAM addresses in SESSION2_VERIFIED_FINDINGS.md
(0x2000003A/3C/3E/40 = pitch/roll/throttle/yaw) and poll those the same
way instead.
"""
import socket, sys, time

class OocdRpc:
    def __init__(self, host='127.0.0.1', port=6666):
        self.sock = socket.create_connection((host, port), timeout=5)
    def cmd(self, c):
        self.sock.sendall(c.encode() + b'\x1a')
        data = b''
        while b'\x1a' not in data:
            data += self.sock.recv(4096)
        return data.split(b'\x1a')[0].decode(errors='replace').strip()
    def mdw(self, addr):
        r = self.cmd(f"mdw 0x{addr:x}")
        return int(r.split(':')[1].strip().split()[0], 16)
    def mdb(self, addr, count=1):
        r = self.cmd(f"mdb 0x{addr:x} {count}")
        # format: "0x200000e8: 07 80 80 00 80 00 01 cc "
        vals = r.split(':')[1].strip().split()
        return [int(v,16) for v in vals]

rpc = OocdRpc()
rpc.cmd("halt")
time.sleep(0.1)

print("=== raw incoming RF packet buffer (RAM 0x200000E8, 8 bytes) ===")
buf = rpc.mdb(0x200000E8, 8)
print(' '.join(f'{b:02x}' for b in buf))

print()
print("=== stored pairing signature in flash (0x8FF8, 8 bytes) ===")
sig = rpc.mdb(0x8FF8, 8)
print(' '.join(f'{b:02x}' for b in sig))

rpc.cmd("resume")
