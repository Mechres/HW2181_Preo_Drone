#!/usr/bin/env python3
"""Writes scaled pitch/roll/throttle/yaw values to the four "verified"
RAM cells CONTINUOUSLY (tight loop, not one-off pokes), to test the
Session 3 hysteresis hypothesis: a single SWD write never gets picked up
by the flight loop's real UpdateMotors call, but repeated writes mimicking
a real incoming packet stream might, if the firmware's internal
signal-acquisition state machine needs several consecutive "good-looking"
samples before it starts trusting the data.

IMPORTANT: this can actually spin a motor if the hypothesis is right and
throttle is set above whatever the real arm/idle threshold turns out to
be. Confirm no propellers are attached and someone is watching before
running with a nonzero throttle. Always uses mwh (halfword), never mww --
see SWD_DEBUGGING_GUIDE.md for why mww corrupts these fields.

Usage: continuous_stick_write.py <throttle_raw 0-255> <duration_s>
  e.g.: continuous_stick_write.py 120 8
Ctrl-C stops early and zeros throttle before exiting.

Requires `openocd -f ../openocd_hw2181.cfg` already running.
"""
import socket, time, sys

class OocdRpc:
    def __init__(self, host='127.0.0.1', port=6666):
        self.sock = socket.create_connection((host, port), timeout=10)
    def cmd(self, c):
        self.sock.sendall(c.encode() + b'\x1a')
        data = b''
        while b'\x1a' not in data:
            data += self.sock.recv(4096)
        return data.split(b'\x1a')[0].decode(errors='replace').strip()
    def mwh(self, addr, val):
        return self.cmd(f"mwh 0x{addr:x} 0x{val & 0xFFFF:x}")

PITCH, ROLL, THROTTLE, YAW = 0x2000003A, 0x2000003C, 0x2000003E, 0x20000040

def scale(raw):
    return ((raw * 4) - 400) & 0xFFFF

if len(sys.argv) < 3:
    print(__doc__)
    sys.exit(1)

throttle_raw = int(sys.argv[1])
duration_s = float(sys.argv[2])

rpc = OocdRpc()
print("targets:", rpc.cmd("targets"))
neutral = scale(100)
thr_val = scale(throttle_raw)
print(f"writing throttle=raw{throttle_raw} (0x{thr_val:04x}) continuously "
      f"for {duration_s}s, pitch/roll/yaw held at neutral. WATCH THE MOTORS.")

start = time.time()
writes = 0
try:
    while time.time() - start < duration_s:
        rpc.mwh(PITCH, neutral)
        rpc.mwh(ROLL, neutral)
        rpc.mwh(THROTTLE, thr_val)
        rpc.mwh(YAW, neutral)
        writes += 1
except KeyboardInterrupt:
    print("\nstopped early by user")
finally:
    rpc.mwh(THROTTLE, scale(0))
    print(f"throttle zeroed. did {writes} write cycles in "
          f"{time.time()-start:.1f}s (~{writes/max(time.time()-start,0.01):.0f} Hz)")
