# Custom Radio Firmware Replacement — Scoping

## Why this approach, and not more nRF24 protocol guessing

Session 2 (see `SESSION2_VERIFIED_FINDINGS.md`) proved, with two independent
methods (a RAM value that never updates, a flash pairing record that never
changes) that a plain nRF24L01+ cannot get a packet accepted by the stock
firmware, even with the receiver's CRC check patched out entirely. We swept
channel, data rate, address, bit-order, and packet framing. There is no
remaining *documented* register we haven't already tried, and the drone's
frame format requires a minimum 4-bit trailer after CRC that real Nordic
nRF24L01+ silicon has no way to produce at all, regardless of configuration.

Rather than keep guessing at the stock protocol, invert the problem: **the
HW2181 is a real, fully reprogrammable ARM Cortex-M0 with a proven, working
SWD flash path (see `SWD_DEBUGGING_GUIDE.md`).** We don't have to make our
transmitter speak the drone's protocol — we can make the drone speak ours.

## What stays, what changes

**Keep completely untouched:**
- PID stabilization loop, complementary filter, motor mixing (`Main_Flight_Loop`, `~0x00000eb8` onward)
- IMU (MPU6050) read/fusion code
- Motor PWM timer setup (T16N0/T16N3)
- Battery ADC monitoring / failsafe voltage logic
- Buzzer/LED code (can reuse for our own status signaling)

**Replace:** only the radio receive + packet-to-stick-values code path.

The hook point is exactly the four RAM cells the flight loop already reads
its input from, confirmed by disassembly + literal pool constants this
session:

| Value    | Address      |
|----------|--------------|
| pitch    | `0x2000003A` |
| roll     | `0x2000003C` |
| throttle | `0x2000003E` |
| yaw      | `0x20000040` |

Each is a 16-bit value in the drone's own internal scale
(`(RawByte*4)-400`, verified). Our replacement radio code's only job is to
write correctly-scaled values here on a reasonable cadence. If we do that
faithfully, the flight controller cannot tell the difference between real
stock-protocol data and ours.

**Still needs to be identified before writing code** (next research step,
not yet done): whatever failsafe/"signal valid" flag currently gates the
`UpdateMotors(0,0,0,0)` failsafe path (per `SAFETY_AND_BATTERY_RE.md`,
unverified) — our replacement code must also manage that flag correctly, or
the flight loop will zero the motors believing the signal was lost, even
while we're feeding it fresh values.

## The one open technical question that decides the approach

Does the RF core have any way to hand us **raw received bytes without its
own hardware frame-validation state machine** (the one that currently
requires seeing a trailer field nRF24 can't produce) in the loop at all?

- If **yes** (some "transparent"/raw capture mode exists): we configure the
  RF core once via the existing `RF_Write_Register` helper (`0x192c`,
  reused as-is, not reimplemented) into that mode, and do 100% of framing,
  address matching, and integrity checking ourselves in new code — using a
  checksum scheme we invent (e.g. a plain byte sum or XOR, not tied to the
  hardware CRC at all), so there is nothing left to guess about on either
  side.
- If **no**: we still don't need the ambitious version — the ONLY hardware
  requirement we can't relax is the trailer bit count (minimum 4, per
  `PKTCTRL.TRAILER_LEN`). Everything else (preamble length, address width,
  CRC) is already fully in our control. The fallback plan is to design our
  nRF24 transmitter to intentionally transmit a few extra "don't care"
  bits after its payload+CRC that happen to satisfy the receiver's trailer
  detection, rather than bypassing hardware framing altogether. This is
  more fragile (depends on undocumented trailer-detection tolerance) and
  is the fallback, not the first choice.

**Partial answer found in the datasheet (Chapter 8, "链路控制方式" / Link
Control Mode):** the RF core has two distinct modes, selected by
`MISC1.PACK_LENGTH_EN` (register `0x29`, bit 12):
- **Hardware link control** (`PACK_LENGTH_EN=1`, the datasheet's reset
  default, and presumably what the stock firmware runs since nothing in
  the init sequence we mapped writes register `0x29`): automatic hardware
  CRC, ACK, and frame completion — this is almost certainly what's
  requiring the trailer we can't produce.
- **Software link control** (`PACK_LENGTH_EN=0`): explicitly documented as
  having **no hardware CRC and no ACK support**; the MCU manages FIFO
  half-empty/half-full flags itself (`THRES` register, already mapped).
  This is a real, documented "more raw" mode, not something we'd be
  inventing.

This doesn't yet confirm whether preamble/trailer detection is *also*
link-mode-independent (physical-layer sync could still be enforced
regardless of MAC-layer mode) — that's the remaining unknown. **Next
concrete step:** test live via SWD by calling the existing
`RF_Write_Register` helper (`0x192c`) from a debugger-driven function call
(same technique already proven working for the breakpoint tests this
session) to flip `PACK_LENGTH_EN` to 0 and observe `FIFOSTATUS`/`STATUS1`
behavior against our existing nRF24 transmitter, before committing to
writing a full replacement receive routine.

## Rough shape of the replacement code (once the above is answered)

1. One-time init: reconfigure the handful of RF registers we need
   differently from stock (reusing `0x192c` calls, same pattern as the
   existing init function at `0x1a72` — we have a fully mapped template).
2. A small receive-poll routine (called from the same place the existing
   `0x1cd2`/`0x1bd6` chain is called from, or via the RF IRQ if we find
   it's real): read whatever bytes are available, validate against our own
   simple frame format (fixed 8 bytes: `[magic, pitch, roll, throttle,
   yaw, buttons, chk_lo, chk_hi]`), and on a valid frame, scale each stick
   byte with the *same* `(Raw*4)-400` formula already verified, then store
   into the four RAM cells above.
3. Failsafe handling: track last-good-packet time ourselves; if stale,
   explicitly zero motors the same way the original failsafe does (reuse
   `UpdateMotors(0,0,0,0)`, address already known: `FUN_00001f30`).

## Size/risk budget

- Free flash available: ~6.5KB (two erased regions confirmed this
  session: `0x7550-0x8400` and `0x852F-0x8FE0`). A hand-written receive +
  scaling routine should be well under 1KB; ample margin.
- Development approach: build and test incrementally via the same
  IAP page-program path already proven working (single-page erase +
  reprogram, byte-exact verify against source, full-flash diff against
  original after every change) — never a full-chip reflash, and the
  complete original `fw_full.bin` remains the rollback path throughout.
- This is meaningfully more work than the 2-byte CRC patch, but is now a
  well-bounded, incremental engineering task rather than open-ended
  guessing, because the target we're writing *to* (those 4 RAM cells) and
  the flash budget are both already confirmed.
