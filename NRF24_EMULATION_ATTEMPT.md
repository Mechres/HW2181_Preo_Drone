# nRF24L01 Emulation Attempt — Full Writeup (Negative Result)

**tl;dr: didn't work, and we have two independent hardware-level proofs
that it didn't work, not just "the LED didn't change."** Documented in
full so nobody else spends a weekend re-deriving this. See
`CUSTOM_RADIO_FIRMWARE_PLAN.md` for the direction taken instead.

## Goal

Control the drone (stock firmware, unmodified) using a plain Arduino +
nRF24L01+ module in place of the missing original remote, by emulating
the drone's native radio protocol on the transmit side ("Path B" — as
opposed to replacing the whole flight controller with new hardware).

## What was built

`nrf24_transmitter/nrf24_transmitter.ino` — a serial-console-driven test
transmitter (not a fixed joystick controller) so every protocol parameter
could be tuned and tested live without reflashing:
- `PAIR` / `FLY` / `STOP` / `SEND` — transmission modes
- `SET T|P|R|Y|B <0-255>` — stick/button values
- `CH <n>` / `RATE <250|1000>` / `CRC <0|1|2>` / `ADDR <hex>` / `REV
  <0|1>` (XN297-style bit reversal on/off) / `PKTFMT LEN|TYPE` — every
  protocol parameter independently switchable at runtime
- `RAW <hex>` — send an exact arbitrary 8-byte packet
- `SWEEP` — auto-cycles channel × CRC × data-rate combinations
- `CHIPCHECK` — verifies the nRF24 module itself is detected over SPI

## What was tried (all via the commands above, systematically)

- **Channels**: native 2–6 (both directly and via the discovered +2MHz
  FREQBASE offset correction — see `SESSION2_VERIFIED_FINDINGS.md`)
- **Data rates**: 250kbps, 1Mbps
- **CRC**: hardware 16-bit, hardware disabled, and manual software CRC16
  with both standard seed conventions (`0xFFFF`, `0x0000`)
- **Address**: `0xE7E7E7E7` (PIPE0 default — confirmed correct later)
- **Bit order**: XN297-style reversed, and native nRF24 order
- **Packet format**: both the original (wrong) `byte0=TYPE` theory and
  the corrected `byte0=0x07 length header` (see findings doc)
- **Power**: nRF24 on a dedicated bench 3.3V supply with decoupling
  capacitor (ruled out brownout as a factor early)

None of these, in any combination, produced a reaction.

## The CRC-bypass patch — eliminating CRC as a variable entirely

Since the drone's own CRC seed couldn't be determined from the firmware
(only the polynomial was confirmed), and the receive routine visibly
gates on it, we patched it out rather than keep guessing. See
`PATCH_CRC_BYPASS.md` for the full patch record. This proved the SWD
flash-write path end-to-end (erase, verify-erased, program 256 words,
verify byte-for-byte) — valuable independent of the outcome below.

**Even with the drone's CRC check fully disabled, reception still never
succeeded.**

## Two independent, decisive negative proofs

Breakpoint-based testing (entering the packet-processing functions) was
tried first and turned out to be a **false-positive-prone method** — the
candidate functions fire on every main-loop tick regardless of real RF
activity (confirmed by breakpointing them with the transmitter completely
unplugged, and seeing them still fire instantly). Lesson documented in
`SWD_DEBUGGING_GUIDE.md`.

The two tests that actually mean something:

1. **RAM value check.** The drone's flight loop reads its throttle input
   from a known RAM address (`0x2000003E`, derived from disassembly —
   see findings doc). We set a distinctive, impossible-to-occur-by-chance
   throttle value (`raw=199` → internal value `0x018C`) and transmitted
   it continuously for 15 seconds while polling that address live over
   SWD. **It read a constant `0x0` the entire time** — never updated.

2. **Persistent flash record check.** The drone had a genuine prior
   pairing record in flash (controller ID `0x01CC`, discovered while
   verifying the hop-channel formula — this was *not* something we
   caused). We switched our test transmitter to a deliberately different
   ID (`0x1122`, hop channel `0x3B`, clearly distinguishable from the
   existing `0x35`) and transmitted continuously for 18+ seconds. **The
   stored record was unchanged** — still `01 CC` / channel `0x35`.

Both are events that only happen on genuine successful reception, and
neither is confounded by the main-loop-noise problem that broke the
breakpoint approach. Two independent methods, same answer: no packet from
our transmitter was ever accepted, under any tested parameter
combination, even with CRC checking removed entirely.

## Leading hypothesis for why

`PKTCTRL.TRAILER_LEN` is set to `0b000` = **4 bits** — the *shortest*
option this chip's register supports, but not zero, and appended after
the payload+CRC in the hardware frame. A real Nordic nRF24L01+ has no
register or mode that produces a custom trailer at all; it simply stops
transmitting after the CRC bytes. If the receive state machine's frame-
completion logic depends on seeing those trailer bits (a reasonable guess
for hardware "Link Control Mode" — see the findings doc's note on
`MISC1.PACK_LENGTH_EN`), that's a hard architectural mismatch no amount
of nRF24-side configuration can close.

This is a hypothesis, not independently proven the way the two negative
results above are — we stopped pursuing nRF24-side fixes once the
evidence made further blind tuning look like a poor use of time, in favor
of the firmware-replacement direction in `CUSTOM_RADIO_FIRMWARE_PLAN.md`.
