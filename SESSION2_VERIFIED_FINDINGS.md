# Session 2 Findings — Independent Verification, SWD Access, and a Firmware Patch

This document covers a follow-up investigation on top of the original
Gemini-CLI/Ghidra reverse-engineering effort in this repo. The original
author was explicit that the findings weren't from a professional security
researcher and asked for verification — this session did that: every claim
below marked **VERIFIED** was independently confirmed either by hand-
disassembling the actual firmware bytes, or by reading real data off the
live chip over SWD. Claims marked **CORRECTED** are places the original
docs (`RADIO_PROTOCOL_RE.md`, `RADIO_EMULATION_NOTES.md`,
`REVERSE_ENGINEERING_NOTES.md`, `SAFETY_AND_BATTERY_RE.md`) turned out to
be wrong, with the actual evidence.

## How to independently re-check anything here

```
arm-none-eabi-objdump -D -b binary -m arm --disassembler-options=force-thumb fw_full.bin
```
Every address below is a raw file offset into `fw_full.bin` (flash is
memory-mapped starting at `0x00000000`, so file offset == runtime address
for flash contents).

## VERIFIED: Packet format

- 8-byte packet. **Byte 0 is a length header (`0x07`), not a `TYPE` field**
  — `RADIO_PACKET_SPEC.md`'s "byte0=TYPE(0x01/0x02)" claim is **CORRECTED**
  here. Proof: the receive routine at `0x1bd6` reads exactly 1 byte from
  the RF FIFO into `buffer[0]`, then immediately uses that byte's *value*
  as the count for a second FIFO read into `buffer[1..]`:
  ```
  1be4: movs r2, #1            ; count=1
  1be8: movs r0, #0x32         ; FIFO0DATA register
  1bea: bl   0x19dc            ; read 1 byte into buffer[0]
  1bee: ldrb r2, [r4, #0]      ; r2 = buffer[0]  <- used as a length!
  1bf4: bl   0x19dc            ; read r2 more bytes into buffer[1..]
  ```
  This matches `RADIO_EMULATION_NOTES.md`'s "Byte 0: 0x07 (Length
  indicator)" claim, which was right.

- Stick scaling: `Internal = (RawByte * 4) - 400`. **VERIFIED** at
  `0x25a2`: `lsls r3,r0,#2` (×4) then `subs r3,#255` then `subs r3,#145`
  (255+145=400 — split into two subtracts because Thumb's immediate
  `subs` only takes an 8-bit constant; this is a real compiler artifact,
  not something a fabricated report would reproduce). Applied identically
  to 4 consecutive fields (pitch/roll/throttle/yaw).

- Processed stick values live in RAM (not just the raw RX buffer) at:
  `pitch=0x2000003A, roll=0x2000003C, throttle=0x2000003E, yaw=0x20000040`
  (16-bit each), derived from the literal pool constant at file offset
  `0x2850` (value `0x20000028`) plus the `+18/+20/+22/+24` byte offsets
  used in the scaling function at `0x2582`. This is the write target the
  *packet-scaling function itself* uses — confirmed by disassembly, not
  in dispute.
  **Session 3 correction:** live SWD testing found this is *not*
  sufficient as a direct write target for a replacement radio/flight
  controller as originally assumed — see `SESSION3_FULLCHAIN_TEST.md`.
  A breakpoint+register trace on the real `UpdateMotors` call showed it
  consistently receives `0,0,0,0` regardless of writes to these cells,
  in the drone's current never-paired state; the actual live input to
  that call is a separate stack-local buffer that a one-off SWD write
  doesn't appear to reach. Likely needs sustained/repeated writes (not
  single pokes) to satisfy an internal hysteresis state machine — see
  that doc for the evidence and the follow-up test needed to confirm.

## VERIFIED: Hop channel formula

`Channel = ((ID_HI + ID_LO) & 0x1F) + 40`. **VERIFIED twice independently:**

1. Disassembly at `0x2518`: `adds r0,r0,r1` then `lsls r0,r0,#27` /
   `lsrs r0,r0,#27` then `adds r0,#40`. The shift-left-27-then-right-27
   pair is the standard ARMv6-M idiom for masking the low 5 bits (`&
   0x1F`), needed because Thumb-1 has no immediate-AND instruction at
   all (only register-register `ands`). Same sequence appears twice
   (`0x2518` and `0x2578`).
2. Cross-checked against **real stored data**: bytes at flash file offset
   `0x8FF8` are literally `51 35 50 69 01 CC 00 00`. Per
   `PERSISTENT_STORAGE_RE.md`'s layout (byte1=channel, bytes4-5=ID),
   `(0x01+0xCC)&0x1F + 40 = 0x35`, exactly matching the stored channel
   byte. This 8-byte sequence is a **real prior pairing record** from
   before this investigation started (controller ID `0x01CC`) — not
   sample/placeholder data.

## VERIFIED: RF address and framing (from the real datasheet, not guessed)

`HW2181_datasheet_cn.md` in this repo is the genuine Eastsoft datasheet
(confirmed: matches the real part number, RAM/flash sizes, and — this
session found the clincher — its documented internal SPI0-to-RF wiring
table (`PA11=IRQ, PA12=MISO, PA13=MOSI, PA14=CSN, PA15=SCK, PA16=CE`)
matches `HW2181_PINOUT.md`'s claims exactly).

- **RF address**: the datasheet's own worked example gives the chip's
  factory-default pipe addresses: `PIPE0 = 0xE7E7E7E7E7E7`, `PIPE1 =
  0xC2C2C2C2C2C2`. `0xE7E7E7E7E7` is *also* the well-known Nordic
  nRF24L01 factory-default address — this SoC's RF core appears to be
  register-compatible with nRF24 by design, not coincidence.
- **PIPE1 is explicitly disabled** — **CORRECTED** vs. the earlier
  assumption that both PIPE0/PIPE1 were enabled (that was just the
  chip's power-on default; the firmware overrides it). Verified at
  `0x1b2e`: `01 21 09 03 3c 20` = `movs r1,#1; lsls r1,r1,#12; movs
  r0,#0x3C(PIPECTRL); call RF_Write` → `PIPECTRL=0x1000`, and per the
  datasheet's bitfield table, bit 12 is `P0_EN` — every other pipe-enable
  bit is 0.
- **PKTCTRL = 0x4800** (register `0x20`, verified at `0x1ad4`-`0x1adc`):
  decoded against the datasheet's bitfield table this is
  `PREAMBLE_LEN=6 bytes, SYNCWORD_LEN=32-bit, TRAILER_LEN=4 bits (the
  chip's minimum available), PACK_TYPE=NRZ, FEC=off`. So the real on-air
  address is the address truncated to 4 bytes: **`0xE7E7E7E7`**.
- **MISC0 = 0x0300** (register `0x23`, verified at `0x1ae0`-`0x1ae6`) —
  this one was actually right in the original docs.
- **"Sync word 55 AA" claim in `REVERSE_ENGINEERING_NOTES.md` is
  WRONG** — **CORRECTED**. Those bytes exist in the binary, but as part
  of an unrelated 32-bit SCU protection/unlock key
  (`SAFETY_AND_BATTERY_RE.md`'s "ADC Magic Key 0x55AA6996" — itself
  slightly off; the real in-memory byte order gives `0xAA559669`, not
  `0x55AA6996`, digits present but reordered). This looks like the
  original AI pattern-matched "Bayang-family toy protocols often use a
  0x55/0xAA-ish sync word" (a true fact in general) onto unrelated bytes
  in *this specific* binary, rather than verifying it here.
- **FREQBASE defaults to 2402 (MHz), not 2400** — never touched by the
  firmware (no write to register `0x25` found anywhere). This means a
  plain nRF24L01's channel numbering (which assumes a 2400MHz base) is
  offset by +2 from this chip's own channel numbers. E.g. "channel 3" in
  this chip's numbering = 2405MHz = **channel 5** on a stock nRF24L01.
  Not mentioned in any of the original docs.
- **FOCCFG (register `0x24`, receive-side preamble detection length) is
  never written either** — stays at its datasheet default `0x4000`,
  which is `PREAMBLE_NUM=4` → detection length = `4*2=8 bits = 1 byte`.
  This is short enough to plausibly match a stock nRF24L01's fixed
  preamble, which is why the "preamble mismatch" theory below was
  revised mid-session (see the nRF24 writeup).
- **CRC**: datasheet confirms CRC16 poly `x^16+x^12+x^5+1` (register
  `0x29` MISC1 bit 14 selects CRC8 vs CRC16) — same polynomial as
  nRF24L01's own hardware CRC16, but the firmware's actual seed
  (`CRC_INIT_DATA`, MISC1 bits 7:0) was not confirmed to differ from
  nRF24's fixed hardware seed, and turned out not to matter in practice
  (see below — CRC was bypassed entirely and reception still failed).
- **Link control mode**: `MISC1.PACK_LENGTH_EN` (bit 12, register `0x29`)
  selects "Hardware Link Control" (automatic CRC/ACK/framing) vs
  "Software Link Control" (FIFO-flag-driven, MCU handles CRC itself, no
  ACK). Register `0x29` is never written by the firmware, so it's on
  Hardware mode (the datasheet default). This is a real, documented
  lever for the custom-firmware plan — see `CUSTOM_RADIO_FIRMWARE_PLAN.md`.

## VERIFIED: IAP flash programming (live, on real hardware)

Full IAP register map, unlock sequence, and erase/program trigger values
extracted from the datasheet's Chapter 3 (IAP base `0x40000800`:
`IAP_CON+0x00, IAP_ADDR+0x04, IAP_DATA+0x08, IAP_TRIG+0x0C, IAP_UL+0x10,
IAP_STA+0x14`; unlock=`0xA5`; page erase trigger=`0x5EA1`; word program
trigger=`0x5DA2`). Confirmed working end-to-end this session: a 2-byte
patch was erased, reprogrammed, and verified byte-for-byte against source
— see `PATCH_CRC_BYPASS.md`.

The datasheet also documents 3 ROM-resident helper functions that would
reduce code size for any future firmware that needs to self-program
flash. Their entry-point addresses were lost in the first-pass PDF-to-
markdown conversion but recovered once the actual datasheet PDF was
found and added to this repo — each address holds a pointer to the
function, not the function itself:

| Function | Pointer stored at |
|---|---|
| `IAP_WordsProgram` | `0x10000000` |
| `IAP_PageErase` | `0x10000004` |
| `IAP_WordProgram` | `0x10000008` |

Not needed for what's been done so far (external IAP register
programming via SWD works fine — see `swd_tools/iap_program_page.py`),
but useful for any future on-chip self-programming firmware.

## VERIFIED: SWD access works

See `SWD_DEBUGGING_GUIDE.md` for the full setup. Headline: `SWD DPIDR
0x0bb11477` (standard ARM CoreSight SW-DP), `Cortex-M0 r0p0` detected,
and a full flash readback via SWD was byte-for-byte identical to
`fw_full.bin` — confirming every finding above applies to this exact
physical chip, not a different revision.

## The nRF24L01 emulation attempt — thorough, and it didn't work

Full writeup in `NRF24_EMULATION_ATTEMPT.md`. Summary: built a serial-
console-driven nRF24L01 transmitter (Arduino sketch in
`nrf24_transmitter/`), swept channel/data-rate/CRC-mode/packet-format/
bit-order combinations, then — after the packet-format correction above —
went further and **patched the drone's own CRC-rejection branch out
entirely** (see `PATCH_CRC_BYPASS.md`) so a CRC mismatch could no longer
be the blocker. Verified via two independent live checks (a RAM location
that should hold the received throttle value, and the persistent flash
pairing record) that **no packet was ever accepted**, under any tested
combination. The likely remaining blocker is the 4-bit trailer field
(`PKTCTRL.TRAILER_LEN`, minimum value on this chip) that real Nordic
nRF24L01+ silicon has no way to produce regardless of configuration.

**Update — root cause found, and it closes this door entirely.** Once the
full HW2181 datasheet PDF was located (see repo root:
`6360290408807600062102767353.pdf`, extracted to
`hw2181_full_datasheet.txt`), its real RF electrical spec was checked:
GFSK frequency deviation = **250kHz** (both 250Kbps and 1Mbps modes).
Checked against the real Nordic nRF24L01+ Product Specification:
deviation = **±160kHz**. This is a hardware-fixed analog mismatch,
unrelated to and beneath any register configuration or firmware logic on
either chip — full details and the live-hardware proof that led to
checking it (the `PIPE_ADDR_MATCH` sweep) are in
`NRF24_EMULATION_ATTEMPT.md`. **This also invalidates
`CUSTOM_RADIO_FIRMWARE_PLAN.md`'s premise** — see that file's updated
header. A plain nRF24L01+ cannot control this drone under any firmware
on either side; a working alternative needs genuine ~250kHz GFSK
deviation (real XN297L hardware, or an SDR configured to match).
