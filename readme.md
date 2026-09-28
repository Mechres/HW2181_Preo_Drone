# Preo RQ77-14W Reverse Engineering Project (Eastsoft HW2181FHNQ)

This repository contains the results of a reverse engineering effort on the Preo RQ77-14W drone firmware. The project was facilitated using **Gemini CLI** in conjunction with **Ghidra**.


> **Note:** This project is a personal exploration and not the work of a professional security researcher. The findings are provided as-is for educational and research purposes.

---

## 🔬 Session 2: Independent Verification + Live Hardware (start here)

The original findings below were AI-assisted and explicitly not verified
by a professional — a second pass checked them against the actual
firmware bytes and, later, the real chip over SWD. **Start with these:**

- [`SESSION2_VERIFIED_FINDINGS.md`](SESSION2_VERIFIED_FINDINGS.md) — what
  was independently confirmed (with the actual disassembly/data as
  evidence), and what turned out to be wrong in the original docs below
  (the RF sync-word claim, the PIPE0/PIPE1 enable claim, the packet
  byte-0 TYPE-vs-length claim, and others).
- [`SWD_DEBUGGING_GUIDE.md`](SWD_DEBUGGING_GUIDE.md) — working SWD setup
  (Pi Pico/picoprobe + OpenOCD) against the real chip, plus tools in
  [`swd_tools/`](swd_tools/).
- [`NRF24_EMULATION_ATTEMPT.md`](NRF24_EMULATION_ATTEMPT.md) — a full,
  systematic attempt to control the stock firmware with a plain
  nRF24L01+ module (code in [`nrf24_transmitter/`](nrf24_transmitter/)).
  **Result: doesn't work, and can't — root cause found.** A real,
  hardware-fixed GFSK deviation mismatch (nRF24L01+: ±160kHz, this chip:
  250kHz — both confirmed against their real datasheets) means no
  firmware configuration on either side can make this pairing work.
  Proven three independent ways on real hardware. Read this before
  trying the same approach.
- [`PATCH_CRC_BYPASS.md`](PATCH_CRC_BYPASS.md) — a verified, reproducible
  2-byte firmware patch, applied and confirmed on real hardware. Proves
  the SWD flash read/write/verify path works end to end (independently
  useful, even though it didn't end up being the actual blocker above).
- [`CUSTOM_RADIO_FIRMWARE_PLAN.md`](CUSTOM_RADIO_FIRMWARE_PLAN.md) —
  scoped, but its premise (fix via a drone-side firmware rewrite) does
  **not** survive the deviation-mismatch finding above. Only relevant
  again if paired with a transmitter that has genuine ~250kHz GFSK
  deviation (real XN297L hardware, or a suitably configured SDR).
- The full genuine HW2181 datasheet PDF (`6360290408807600062102767353.pdf`)
  is now in this repo too (a community member tracked it down), along
  with its full text extraction (`hw2181_full_datasheet.txt`) — the
  earlier per-topic `.md` files only had partial/reflowed excerpts of it.

## 🔬 Session 3: Full-Chain Test (does an SWD write actually spin a motor?)

- [`SESSION3_FULLCHAIN_TEST.md`](SESSION3_FULLCHAIN_TEST.md) — live test
  of `CUSTOM_RADIO_FIRMWARE_PLAN.md`'s core assumption (write the four
  scaled stick-value RAM cells, the stock flight loop picks them up).
  **Result: not confirmed as described.** A breakpoint+register trace on
  the real `UpdateMotors` call showed it always receives `0,0,0,0` in the
  drone's current never-paired state, regardless of writes to those
  cells — the real live input is a separate stack-local buffer a one-off
  write doesn't appear to reach. Leading hypothesis: an internal
  signal-acquisition hysteresis state machine needs *sustained* writes
  (mimicking a real packet stream), not single pokes — not yet confirmed,
  see the doc for the concrete next test. Also documents two real
  debugging gotchas found along the way: hardware watchpoints appear
  non-functional on this target, and `mdw`/`mww` (word-width) silently
  fail/corrupt on these not-word-aligned RAM cells — always use
  `mdh`/`mwh` (halfword) instead. `SWD_DEBUGGING_GUIDE.md` and
  `SESSION2_VERIFIED_FINDINGS.md` have been updated with pointers to
  this correction.

---

## 🚀 Latest Findings Summary
The reverse engineering process is complete. We have successfully mapped the architecture of the **HW2181 SOC (ARM Cortex-M0)**:
*   **Radio Protocol:** Identified the pairing mechanism on Channel 3 and the adaptive frequency hopping algorithm.
*   **Packet Specification:** Fully decoded the 8-byte control packet, including stick scaling (`(Raw * 4) - 400`) and button bitmasks.
*   **Flight Logic:** Located the PID stabilization loop and MPU6050 IMU integration via I2C.
*   **Safety Systems:** Decoded multi-level battery monitoring (ADC) and signal-loss failsafe logic.
*   **Hardware Pinout:** Mapped the QFN48 package for physical debugging (UART @ 19200 baud, SWD, PWM).
*   **Persistent Storage:** Decoded the setting signature used to store Pairing IDs and IMU calibration in Flash memory.
*   **RF Emulation:** Documented the specific requirements for XN297L compatibility (bit-reversal, length header).

---

## 💡 Visual Language & Special Maneuvers

### Visual Language (LED Patterns)
Controlled via GPIO (Pins 12 & 13):
*   **Fast Blink:** Pairing Mode.
*   **Slow Blink:** Low Battery Warning.
*   **Solid:** Connection established.
*   **Fast Periodic Burst:** Signal loss failsafe active.

### Special Maneuvers
*   **3D Flip:** Triggered by deflection + Byte 5 bitmask.
*   **Auto-Takeoff:** Pre-programmed PWM ramp-up.
*   **IMU Calibration:** Zero-offset routine for MPU6050.

---

## 📂 File Manifest

### Firmware & Memory Dumps
*   `fw_full.bin`: Complete original firmware dump (rollback reference — confirmed via live SWD readback to byte-for-byte match the chip before any patching).
*   `fw_patched_crcbypass.bin`: `fw_full.bin` with the CRC-bypass patch applied (see `PATCH_CRC_BYPASS.md`) — reflects what's currently on the chip as of Session 2.
*   `ram_live.bin`: Live RAM dump from operation.
*   `fw_full.bin.gzf`: **Recommended Ghidra Project Export** (contains all labeled functions).

### Session 2 tools
*   `openocd_hw2181.cfg`: working generic Cortex-M0 SWD config for this chip (no vendor target script exists in OpenOCD).
*   `swd_tools/`: Python scripts for IAP flash programming, RAM/flash inspection, and breakpoint testing over a live OpenOCD session — see `swd_tools/README.md`.
*   `nrf24_transmitter/`: serial-console-driven Arduino nRF24L01+ test transmitter — see `NRF24_EMULATION_ATTEMPT.md` for why it doesn't work against the stock firmware.

### Technical Specifications
*   [`RADIO_PACKET_SPEC.md`](RADIO_PACKET_SPEC.md): Packet bitmasks and scaling formulas.
*   [`HW2181_PINOUT.md`](HW2181_PINOUT.md): Physical pin mapping and test points.
*   [`RADIO_PROTOCOL_RE.md`](RADIO_PROTOCOL_RE.md): Hopping and pairing logic.
*   [`FLIGHT_LOGIC_RE.md`](FLIGHT_LOGIC_RE.md): PID loop and IMU processing.
*   [`SAFETY_AND_BATTERY_RE.md`](SAFETY_AND_BATTERY_RE.md): ADC thresholds and failsafes.
*   [`PERSISTENT_STORAGE_RE.md`](PERSISTENT_STORAGE_RE.md): Flash-based settings analysis.
*   [`RADIO_EMULATION_NOTES.md`](RADIO_EMULATION_NOTES.md): Hurdles for NRF24L01+ emulation (XN297 compatibility).


### (Old) General Documentation
*   `DRONE_RE_FINAL_REPORT.md`: Comprehensive project summary.
*   `REVERSE_ENGINEERING_NOTES.md`: Raw technical notes.


---

## 🛠 Tools Used
*   **Gemini CLI**: AI-assisted analysis and automation.
*   **Ghidra**: Reverse engineering suite (core functions now 90% labeled).

---

## 📜 Disclaimer
The information and files in this repository are for research purposes only. Use the firmware or memory dumps at your own risk.

## Contribution
If you can test, verify it would be really helpfull, also if you can find pdf version of the datasheet please send it or add to this repository.
