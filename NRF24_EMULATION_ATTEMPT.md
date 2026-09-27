# nRF24L01 Emulation Attempt — Full Writeup (Negative Result)

**tl;dr: doesn't work, can't work, and we know exactly why — a real,
hardware-fixed GFSK deviation mismatch (160kHz vs 250kHz) between
nRF24L01+ and this chip's RF core, confirmed against both real
datasheets.** Documented in full, including three independent negative
proofs on real hardware, so nobody else spends a weekend re-deriving
this. `CUSTOM_RADIO_FIRMWARE_PLAN.md`'s premise (fixable via a firmware
rewrite) does not survive this finding — see its update.

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

## Round 2: live SWD register probing, and a decisive third proof

After the two negative results above, we got live SWD access working
against the running chip (see `SWD_DEBUGGING_GUIDE.md`) and pushed
further:

- Patched out the drone's CRC-mismatch rejection branch entirely (see
  `PATCH_CRC_BYPASS.md`) — reception still failed.
- Found a much more sensitive, lower-level signal than anything above:
  `STATUS1` register bit 7, `PIPE_ADDR_MATCH` — set by the RF core's own
  hardware address correlator, upstream of *any* software (hardware or
  software link-control mode alike). Read it live via a debugger-injected
  function call to the firmware's own `RF_Read_Register` (`0x1962`),
  restoring all clobbered registers afterward so the running firmware
  never sees the interruption.
- Swept address width (3/4/5 bytes), bit-reversal on/off, both link
  control modes (`MISC1.PACK_LENGTH_EN`), multiple channels, both data
  rates, both CRC states — **`PIPE_ADDR_MATCH` never set, in any single
  reading, across every combination tested.**

That's the third independent hardware-level proof, and the most telling
one: if the RF core's own correlator never recognizes our transmission,
no downstream software change (on either the nRF24 side or a rewritten
drone firmware) could matter, because there'd be nothing valid to hand to
that software in the first place.

## Root cause found: GFSK deviation mismatch (hardware, unfixable by configuration)

Once the full HW2181 datasheet PDF was available (see repo root), its RF
electrical spec was checked directly:

> `Δf1M` / `Δf250K` (frequency deviation) = **250 kHz** (Typ.), at both
> 1Mbps and 250Kbps.

Checked against the real Nordic nRF24L01+ Product Specification:

> Frequency deviation @ 250kbps / 1Mbps = **±160 kHz**.

**160kHz vs 250kHz is a genuine, hardware-fixed GFSK modulation
mismatch.** Deviation is set by each chip's analog RF frontend, not
adjustable by any register on either side. This fully explains every
result above: the drone's receiver is tuned to demodulate a ±250kHz
signal, and a ±160kHz nRF24L01+ transmission is enough off-spec that its
address correlator never reliably locks on — independent of address,
channel, CRC, packet format, or link-control mode, exactly matching what
was observed.

**Conclusion: a plain nRF24L01+ cannot control this drone, under any
firmware configuration on either side.** This also invalidates
`CUSTOM_RADIO_FIRMWARE_PLAN.md`'s core premise (that a firmware rewrite
on the drone could work around the mismatch) — that plan assumed a
digital/framing problem; this is analog, beneath any firmware on either
chip. A working alternative transmitter needs genuine ~250kHz GFSK
deviation — either real XN297L-family hardware, or an SDR (e.g. HackRF,
ADALM-PLUTO) configured to match.
