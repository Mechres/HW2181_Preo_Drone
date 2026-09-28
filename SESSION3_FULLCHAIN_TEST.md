# Session 3 — Full-Chain Test: Does an SWD RAM Write Actually Spin a Motor?

Follow-up to `CUSTOM_RADIO_FIRMWARE_PLAN.md`'s premise (write scaled stick
values directly into the four RAM cells the packet handler writes, and let
the stock flight loop drive the motors from them). This session tested that
premise empirically, on real hardware, live over SWD. **Result: no, not as
originally planned — the premise needs a correction.**

## Setup

Persistent `openocd -f openocd_hw2181.cfg` session, target left running
(never halted for writes — background/live `mww`/`mwh` writes only, per
`SWD_DEBUGGING_GUIDE.md`). No blades attached throughout (confirmed by the
project owner before every test in this session).

## Test 1: write scaled throttle values, watch for any motor response

Wrote `pitch/roll/yaw = neutral (raw=100 -> internal=0)` and ramped
`throttle` from raw 0 to 180 in steps of 10, 1s apart, via live
**halfword** writes (`mwh`, matching the field's real 16-bit width — see
the bug note below) to `0x2000003E`. **No motor response at all**, even at
the top of the ramp (raw=180 -> internal=+320, well above any plausible
arm threshold).

### Bug found and fixed mid-test: `mww` vs `mwh`

The first attempt used `mww` (32-bit word write) on these fields. The four
fields are only **2 bytes apart** in RAM (`0x3A, 0x3C, 0x3E, 0x40`), so
every `mww` actually wrote 4 bytes and stomped the *next* field's low
16 bits too (e.g. writing throttle at `0x3E` also zeroed yaw's low byte at
`0x3E-0x41` overlap). Re-ran with `mwh` (halfword write, matching the
verified 16-bit field width) — same result, no motor response. This
confirms the null result isn't just an artifact of the alignment bug, but
it's a real bug worth flagging for anyone reusing these scripts: **always
use `mwh` for these fields, never `mww`.**

## Test 2: trace the real motor-update call live, with a breakpoint

Found two call sites of `UpdateMotors` (`0x1f30`) in the flight-loop
function (which starts well before `0x1600` and runs to past `0x1800` —
one large function, no sub-function boundaries in between):

- **`0x168a`** — an explicit `UpdateMotors(0,0,0,0)` failsafe call, reached
  when an internal bad-signal counter (a byte at `[r6+4]`, `r6` = a fixed
  RAM struct pointer at literal `0x17c4`) exceeds 20.
- **`0x178a`** — the "real" call, arguments loaded via `ldrsh` from a
  **stack-local** buffer at `sp+0x1c0` (offsets `+22/+24/+26/+28` for
  pitch/roll/throttle/yaw respectively — NOT the same as the global RAM
  cells), optionally clamped to a minimum of 100 first (gated by a byte at
  `[sp+0x200+13]`).

Set a hardware breakpoint at `0x178a` (confirmed working — breakpoints are
reliable on this target, unlike watchpoints, see below) and read `r0-r3`
(the actual call arguments: `r0=yaw, r1=throttle, r2=roll, r3=pitch`, per
AAPCS + the load order) on each hit, while writing `throttle=raw120` via
SWD beforehand.

**Result: the call fires reliably (~1.5-2 Hz), but `r0=r1=r2=r3=0x00000000`
on every single hit, regardless of what was written to the global RAM
cells.** Since the min-100 clamp block wasn't visited (confirmed
separately — see below), this rules out "clamped to 100" and confirms the
stack-local buffer itself holds raw zero at call time, unaffected by our
write to the global cells.

## Test 3: does `0x1422` (pitch's known static read of `0x2000003A`) ever run?

Session 2 R.E. documented a direct read of the global pitch cell in this
function (disassembly at file offset `0x141e-0x1422`: loads
`r2 = 0x2000003A`, reads `ldrsh r0, [r2, #0]`, compares against `±200` to
decide if this looks like a real vs. garbage/never-initialized value —
apparently a "does the raw signal look sane" gate, feeding into a
searching/pairing-vs-locked state machine using RAM struct fields at
`0x20000000+{6,8,10,12,14}` and further gating flags).

Set a hardware breakpoint directly at `0x1422` and let it run for 10s
(hundreds of expected UpdateMotors cycles' worth of time). **Zero hits.**
This instruction — despite being real code that plainly reads the pitch
global cell in the static disassembly — **does not execute at all** during
normal steady-state operation with no radio ever paired. So this code path
is not part of the per-tick hot loop that reaches `UpdateMotors`; it's
reached some other way (perhaps once at a state transition that never
occurs while unpaired, perhaps from a different, not-yet-identified
predecessor). **Static disassembly alone was actively misleading here** —
it correctly found a real read of the global cell, but wrongly implied (by
proximity/apparent flow) that this is on the hot path. It isn't, at least
not in the "never paired" state this board is currently in.

## Gotcha: hardware watchpoints appear non-functional on this target

Before finding breakpoints were the reliable tool, this session tried
**watchpoints** (`wp <addr> <len> <r|a>`) to catch reads of the throttle
and pitch cells non-invasively. **Zero hits, every time — including a
sanity check on `0x200000E8` (the raw RF packet buffer, known to be
touched constantly from the surrounding disassembly) with `a` (any
access).** Breakpoints on the exact same kind of target work fine (proven
in Test 2 and Test 3 above), so this isn't a general debug-access problem
— specifically the DWT-based watchpoint mechanism seems not to fire on
this chip via this OpenOCD generic Cortex-M0 config. Possibly related:
the connection log shows `DWT_DEVARCH: 0x0` and `cmsis_dap.c: refusing to
enable sticky overrun detection` at attach time — this DWT block may be a
nonstandard/reduced implementation on this SoC. **Don't trust a "0 hits"
result from `wp` on this target as meaningful — verify with a breakpoint
on a known static address instead**, the way Test 3 did.

## What this means for the plan in `CUSTOM_RADIO_FIRMWARE_PLAN.md`

The plan's core assumption — "write the four scaled RAM cells
(`0x2000003A/3C/3E/40`), the stock flight loop will pick them up and drive
the motors, we don't need to understand more than that" — **is not
supported by live evidence.** Those cells ARE confirmed written by the
packet-scaling function when a real packet is decoded (Session 2 finding,
still correct). But this session found no evidence they're the live input
to the actual per-tick `UpdateMotors` call while unpaired — the real
argument source is a separate stack-local buffer that stayed at zero
throughout every test here, and the specific static code path that
*would* copy global→local was proven (via breakpoint) not to execute in
this state.

**Two live hypotheses, not yet distinguished:**

1. **Hysteresis/rate hypothesis**: the signal-acquisition state machine
   (the `0x20000000+{6,8,...}` counters) requires *multiple consecutive
   loop passes* of plausible-looking data to transition from
   "searching" to "locked" before it starts copying global→local at all
   — a single one-shot SWD write, even if perfectly scaled, looks
   identical to one stray good sample amid silence and never
   accumulates enough hits. If true, the fix is mechanical: write the
   four cells **continuously**, in a tight loop (say 20-50 Hz), for
   several seconds, mimicking a real incoming packet stream, before
   judging the test a failure.
2. **Wrong-address hypothesis**: the real live input path is something
   else entirely (a different RAM location, or a condition this session
   hasn't found), and the four "verified" cells are close to a dead end
   for this purpose no matter how they're written.

**Recommended next step (needs the owner present to watch for a real
motor response, per this session's standing safety practice):** re-run
Test 1's ramp, but as a **continuous tight-loop write** (not discrete
steps 1s apart) for 5-10 seconds, watching both for a motor response *and*
re-checking the `0x178a` breakpoint trace (Test 2's method) *during* that
continuous-write window to see if `r0-r3` ever stop reading zero. That one
combined test would distinguish the two hypotheses directly, rather than
more static disassembly (which this session's Test 3 showed can be
actively misleading on this binary without also verifying against live
breakpoint hits).

## Tools

Added to `swd_tools/` this session:
`trace_updatemotors.py` (Test 2's breakpoint+register-dump script) and
`continuous_stick_write.py` (a cleaned-up, tight-loop version of Test 1,
ready to run the recommended next test above). Test 3's bare
single-address breakpoint check was just `swd_tools/bp_watch.py`, already
in the repo from Session 2.
