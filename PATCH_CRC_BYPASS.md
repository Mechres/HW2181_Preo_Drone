# Patch: CRC-Rejection Bypass

**Status: applied and verified on real hardware this session.**

## What it does

Removes the drone's own hardware-CRC-mismatch rejection check in the
radio receive routine. Without it, a received packet with a CRC that
doesn't match the chip's internal CRC engine's expectation is no longer
silently dropped before its contents are even read.

This does **not** touch PID/motor/IMU/failsafe logic at all — purely a
2-byte change in the radio receive path.

## The patch

| | |
|---|---|
| File offset | `0x1BE2` (2 bytes) |
| Original bytes | `0c d4` (Thumb: `bmi.n 0x1bfe`) |
| Patched bytes | `00 bf` (Thumb: `nop`) |

Context (disassembly around the patch site):
```
1bda: movs r0, #54 (0x36)     ; FIFO0CTRL register number
1bdc: bl   0x1962              ; RF_Read_Register
1be0: lsls r0, r0, #18         ; shift bit13 (PRX_CRC_ERR0) into sign position
1be2: bmi.n 0x1bfe             ; [PATCHED to nop] originally: skip packet if CRC error
1be4: movs r2, #1               ; (now always reached) begin reading payload
```
`lsls r0,r0,#18` on a 16-bit register value brings original bit 13 into
bit 31 (the sign bit checked by `bmi`); bit 13 of `FIFO0CTRL` is
`PRX_CRC_ERR0` per the datasheet. Replacing the conditional branch with a
`nop` makes execution always fall through to reading the payload,
regardless of the CRC status.

## Why this specific patch (not something else)

At the time this was applied, the CRC seed used by the firmware's
software layer could not be determined (only the polynomial, `x^16+
x^12+x^5+1`, was confirmed — same as nRF24L01's own hardware CRC16, but
seed compatibility was unverified and nRF24 hardware doesn't expose seed
control anyway). Rather than keep guessing seeds, removing the check
entirely turns CRC from an unknown variable into a non-factor, so any
further reception failure could be attributed to something else. See
`NRF24_EMULATION_ATTEMPT.md` for the result (reception still failed even
with this applied — the blocker is elsewhere).

## How it was applied (reproducible)

Full procedure and safety rationale in `SWD_DEBUGGING_GUIDE.md`. Summary:
1. Built `fw_patched_crcbypass.bin` = `fw_full.bin` with only those 2
   bytes changed (verified via Python byte-diff against the original
   before flashing anything).
2. Confirmed `CFG_WORD0` bit 15 (required for SWD-mode flash erase/
   program) was already set — no additional risky config change needed.
3. Erased flash page 6 (`0x1800`-`0x1BFF`, the 1KB page containing the
   patch site — flash can only be erased a whole page at a time).
4. Verified the page actually read back as `0xFFFFFFFF` (erased) with
   `IAP_CON.FLASH_REQ` released (reading flash while it's still held
   returns stale data, not real content — see the debugging guide).
5. Reprogrammed all 256 words of that page from the patched file (so
   everything except the 2 intentionally-changed bytes was restored
   exactly).
6. Verified the page byte-for-byte against the patched file after
   release.
7. **Final full-flash readback** (all 36KB) confirmed exactly 2 bytes
   differ from the original firmware, at exactly the intended offset —
   nothing else on the chip was touched.

## Rollback

`fw_full.bin` in this repo is the complete, unmodified original. To
revert: erase page 6 and reprogram it from `fw_full.bin` instead, using
the same procedure.

## Current firmware state

The drone's flash currently has this patch applied (as of this session).
`fw_patched_crcbypass.bin` in this repo reflects exactly what's on the
chip.
