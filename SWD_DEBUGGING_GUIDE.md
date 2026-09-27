# SWD Debugging Setup — Raspberry Pi Pico (picoprobe) + OpenOCD

Confirmed working this session against a real Preo RQ77-14W board.

## Hardware

Drone board's debug pins (per `HW2181_PINOUT.md`): `CLK` (PA0/SWCLK),
`DAT` (PA1/SWDIO), `RST`, `GND`, `VDD`.

Wiring used (Raspberry Pi Pico flashed with the official `picoprobe` /
"Debugprobe" firmware — pins are fixed by that firmware, not
configurable):

| Pico pin | Drone pin | Notes |
|---|---|---|
| GP2 | CLK | SWCLK |
| GP3 | DAT | SWDIO |
| GND | GND | required, common reference |
| — | RST | **left disconnected** — SWD can attach/reset over the wire protocol alone; wiring a debugger GPIO to a board's own reset line risks fighting its reset circuit for no benefit here |
| — | VDD | **left disconnected** — this is the target's own logic-level reference pin, not something the probe should drive; power the drone from its own battery/PSU during debugging |

GP0/GP1 on stock picoprobe are a UART bridge (for reading target serial
output), unrelated to SWD — not used here.

Power the drone from its own supply (battery or bench PSU) during all
SWD work; don't try to power it from the Pico.

## Verifying the connection

```
lsusb | grep -i debugprobe
# Bus 003 Device 027: ID 2e8a:000c Raspberry Pi Debugprobe on Pico (CMSIS-DAP)
```

## OpenOCD config

The HW2181 isn't a chip OpenOCD ships a target script for, so this repo's
`openocd_hw2181.cfg` is a minimal generic Cortex-M0 bring-up:

```tcl
adapter driver cmsis-dap
transport select swd
adapter speed 1000

set _CHIPNAME hw2181
swd newdap $_CHIPNAME cpu -expected-id 0x0bb11477 -ignore-version
dap create $_CHIPNAME.dap -chain-position $_CHIPNAME.cpu
target create $_CHIPNAME.cpu cortex_m -dap $_CHIPNAME.dap

init
halt
```

`0x0bb11477` is the standard ARM CoreSight SW-DP ID — confirmed correct
for this chip on first try (`Info : SWD DPIDR 0x0bb11477`), consistent
with it being a straightforward ARM-licensed Cortex-M0 implementation.

Run as a persistent session (needed for the scripting approach below):
```
openocd -f openocd_hw2181.cfg
```
This opens a GDB server on port 3333, a Tcl RPC server on port 6666, and
a telnet server on port 4444. **Only one process can hold the USB device
at a time** — if you see `CMSIS-DAP command CMD_INFO failed` / `Input/
Output Error`, kill any other running `openocd` process first.

## Driving it from Python (Tcl RPC)

More reliable for scripted work (register polling with real error
handling) than hand-typing telnet commands:

```python
import socket

class OocdRpc:
    def __init__(self, host='127.0.0.1', port=6666):
        self.sock = socket.create_connection((host, port), timeout=5)
    def cmd(self, c):
        self.sock.sendall(c.encode() + b'\x1a')
        data = b''
        while b'\x1a' not in data:
            data += self.sock.recv(4096)
        return data.split(b'\x1a')[0].decode(errors='replace').strip()
    def mww(self, addr, val):
        return self.cmd(f"mww 0x{addr:x} 0x{val:x}")
    def mdw(self, addr):
        r = self.cmd(f"mdw 0x{addr:x}")
        return int(r.split(':')[1].strip().split()[0], 16)

rpc = OocdRpc()
print(rpc.cmd("targets"))
```

Useful commands via `rpc.cmd(...)`: `halt`, `resume`, `reg pc`,
`bp <addr> 2 hw` (set hardware breakpoint), `rbp <addr>` (remove),
`dump_image <file> <addr> <size>` (read memory to a local file).

## Things that bit us — worth knowing before you repeat this

- **RF "registers" (the chip's own 0x00-0x47 numbered RF core registers,
  documented in the datasheet's Chapter 13) are NOT memory-mapped.**
  They're accessed internally via SPI0, mediated entirely by the running
  firmware's own `RF_Write_Register`/`RF_Read_Register` functions
  (`0x192c`/`0x1962`). You cannot `mdw`/`mww` them directly from the
  debug port. To read/write one live, you'd need to either call those
  existing functions via a debugger-driven function call (halt, set
  r0/r1, set PC, set a breakpoint at the return address, resume, wait),
  or hand-implement the SPI0 transaction yourself against the *real*
  memory-mapped SPI0 peripheral at `0x40008000` — and either way, doing
  this while the firmware is also running risks bus contention with its
  own use of the same peripheral. We did not need this for anything
  accomplished so far; noting it because it's a natural first mistake
  (assuming `mdw 0x40008000+regnum` would just work — it doesn't).
- **Reading flash while `IAP_CON.FLASH_REQ` is still held returns stale/
  wrong data**, not real flash content — release flash access
  (`IAP_CON=0`, wait for `FLASH_ACK` bit to clear) before verifying
  anything you just erased or programmed.
- **Breakpoints on "packet received" style functions are not reliable
  signals of real RF activity** on this firmware — several candidate
  functions turned out to run unconditionally every main-loop tick
  regardless of whether anything was actually received (confirmed by
  breakpointing with the test transmitter completely unplugged and
  seeing it still fire instantly). Reading the actual **processed data
  value** in RAM, or a value that's provably only written on a genuine
  event (like the persistent flash pairing record), is a much more
  trustworthy signal than "was this function entered."
