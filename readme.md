# Preo RQ77-14W Reverse Engineering Project (Eastsoft HW2181FHNQ)

This repository contains the results of a reverse engineering effort on the Preo RQ77-14W drone firmware. The project was facilitated using **Gemini CLI** in conjunction with **Ghidra**.


> **Note:** This project is a personal exploration and not the work of a professional security researcher. The findings are provided as-is for educational and research purposes.

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
*   `fw_full.bin`: Complete firmware dump.
*   `ram_live.bin`: Live RAM dump from operation.
*   `fw_full.bin.gzf`: **Recommended Ghidra Project Export** (contains all labeled functions).

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
