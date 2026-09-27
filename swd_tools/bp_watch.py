#!/usr/bin/env python3
"""Set a hardware breakpoint at a given address via a live OpenOCD Tcl RPC
session, resume the target, and report whether/when it fires within a
watch window. Requires `openocd -f ../openocd_hw2181.cfg` already running.

Usage: python3 bp_watch.py 0x2522 [watch_seconds]

Caveat learned the hard way (see SWD_DEBUGGING_GUIDE.md): many candidate
"packet received" functions on this firmware fire on every main-loop tick
regardless of real RF activity. Confirm any address you use here actually
distinguishes real events by testing with your transmitter unplugged
first -- if it still fires immediately, it's not a useful signal.
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

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    bp_addr = sys.argv[1]
    watch_seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 20.0

    rpc = OocdRpc()
    print("target:", rpc.cmd("targets"))

    print(rpc.cmd("halt"))
    time.sleep(0.2)
    print("setting breakpoint:", rpc.cmd(f"bp {bp_addr} 2 hw"))
    print("resuming target to run normally:", rpc.cmd("resume"))

    print(f"watching for {watch_seconds:.0f} seconds...")
    hit = False
    steps = int(watch_seconds / 0.5)
    for i in range(steps):
        time.sleep(0.5)
        state = rpc.cmd("targets")
        if "halted" in state:
            hit = True
            pc = rpc.cmd("reg pc")
            print(f"[{i*0.5:.1f}s] BREAKPOINT HIT! pc={pc}")
            break
        if i % 4 == 0:
            print(f"[{i*0.5:.1f}s] still running, no hit yet")

    if not hit:
        print(f"No breakpoint hit after {watch_seconds:.0f} seconds.")

    print("removing breakpoint:", rpc.cmd(f"rbp {bp_addr}"))
    print("resuming:", rpc.cmd("resume"))

if __name__ == "__main__":
    main()
