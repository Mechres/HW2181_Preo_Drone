#!/usr/bin/env python3
"""Breakpoints the real per-tick UpdateMotors call site (0x178a) and dumps
r0-r3 (yaw, throttle, roll, pitch -- AAPCS arg order per the load sequence
at 0x1778-0x1788) on every hit, resuming automatically each time.

This is the tool that found the Session 3 result: with the target
unpaired, every hit reads r0=r1=r2=r3=0, regardless of what's written to
the "verified" scaled stick-value RAM cells (0x2000003A/3C/3E/40) -- see
SESSION3_FULLCHAIN_TEST.md. Use this to check whether a *sustained*
write loop (run this at the same time as a separate continuous-write
script) ever unblocks it.

Requires `openocd -f ../openocd_hw2181.cfg` already running (persistent
session, target left running -- do NOT halt it for anything other than
this script's own brief breakpoint stops, which auto-resume).
"""
import socket, time, re, sys

class OocdRpc:
    def __init__(self, host='127.0.0.1', port=6666):
        self.sock = socket.create_connection((host, port), timeout=10)
    def cmd(self, c):
        self.sock.sendall(c.encode() + b'\x1a')
        data = b''
        while b'\x1a' not in data:
            data += self.sock.recv(4096)
        return data.split(b'\x1a')[0].decode(errors='replace').strip()

def reg(rpc, name):
    resp = rpc.cmd(f"reg {name}")
    m = re.search(r"0x([0-9a-fA-F]+)", resp)
    return int(m.group(1), 16) if m else None

BP_ADDR = 0x178a
DURATION_S = float(sys.argv[1]) if len(sys.argv) > 1 else 10
MAX_HITS = int(sys.argv[2]) if len(sys.argv) > 2 else 20

rpc = OocdRpc()
print("targets:", rpc.cmd("targets"))
rpc.cmd(f"rbp 0x{BP_ADDR:x}")  # clear any stale bp from a prior crashed run
rpc.cmd("halt")
time.sleep(0.1)
print("bp set:", repr(rpc.cmd(f"bp 0x{BP_ADDR:x} 2 hw")))
rpc.cmd("resume")

start = time.time()
hits = 0
while time.time() - start < DURATION_S and hits < MAX_HITS:
    rpc.cmd("wait_halt 500")
    if "halted" not in rpc.cmd("targets"):
        continue
    time.sleep(0.03)
    r0, r1, r2, r3 = reg(rpc, "r0"), reg(rpc, "r1"), reg(rpc, "r2"), reg(rpc, "r3")
    hits += 1
    print(f"hit {hits}: yaw(r0)=0x{r0:08x} throttle(r1)=0x{r1:08x} "
          f"roll(r2)=0x{r2:08x} pitch(r3)=0x{r3:08x}")
    rpc.cmd("resume")

rpc.cmd(f"rbp 0x{BP_ADDR:x}")
rpc.cmd("resume")
print(f"done, {hits} hits in {DURATION_S:.0f}s")
