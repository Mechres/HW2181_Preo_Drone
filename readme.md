# Preo RQ77-14W Reverse Engineering Project

This repository contains the results of a reverse engineering effort on the Preo RQ77-14W drone firmware. The project was facilitated using **Gemini CLI** in conjunction with **Ghidra**.

> **Note:** This project is a personal exploration and not the work of a professional security researcher. The findings are provided as-is for educational and research purposes.

---

## 🚀 Latest Findings Summary
The reverse engineering process is complete. We have successfully mapped the architecture of the **HW2181 SOC (ARM Cortex-M0)**:
*   **Radio Protocol:** Identified the pairing mechanism on Channel 3 and the adaptive frequency hopping algorithm based on the Controller ID.
*   **Packet Specification:** Fully decoded the 8-byte control packet, including stick scaling and button bitmasks.
*   **Flight Logic:** Located the PID stabilization loop and the MPU6050 IMU integration via I2C.
*   **Safety Systems:** Decoded multi-level battery monitoring (ADC) and signal-loss failsafe logic.
*   **Hardware Pinout:** Mapped the QFN48 package for physical debugging (UART, SWD, PWM).

---

## 💡 Visual Language & Special Maneuvers

### Visual Language (LED Patterns)
The drone communicates its state through GPIO-controlled LEDs (Pins 12 & 13):
*   **Fast Blink:** Pairing Mode (Scanning Channel 3).
*   **Slow Blink:** Low Battery Warning (Triggered by ADC Stage 1).
*   **Solid:** Connection established and IMU calibrated.
*   **Fast Periodic Burst:** Signal loss failsafe active.

### Special Maneuvers
Decoded from the `Decode_Radio_Commands` logic:
*   **3D Flip:** Triggered by high deflection on Pitch/Roll while the "Flip" bit (Byte 5) is active. The firmware momentarily suspends Z-axis stabilization to allow the rotation.
*   **Auto-Takeoff:** Initiated by Bit 0 of Byte 5. The drone executes a pre-programmed ramp-up of motor PWMs to a hovering duty cycle.
*   **IMU Calibration:** Holding Bit 2 of Byte 5 high while the drone is stationary on a flat surface triggers the zero-offset routine for the MPU6050.

---

## 📂 File Manifest

### Firmware & Memory Dumps
*   `fw_full.bin`: The complete firmware dump.
*   `ram_live.bin`: A live RAM dump captured during operation.

### Technical Specifications (New)
*   [`RADIO_PACKET_SPEC.md`](RADIO_PACKET_SPEC.md): Full 8-byte packet bitmask and stick scaling formulas.
*   [`HW2181_PINOUT.md`](HW2181_PINOUT.md): QFN48 Pinout and internal MCU-to-RF connections.
*   [`RADIO_PROTOCOL_RE.md`](RADIO_PROTOCOL_RE.md): Detailed frequency hopping and pairing logic.
*   [`FLIGHT_LOGIC_RE.md`](FLIGHT_LOGIC_RE.md): Breakdown of IMU processing and PID motor control.
*   [`SAFETY_AND_BATTERY_RE.md`](SAFETY_AND_BATTERY_RE.md): ADC-based safety thresholds and failsafe mechanisms.

### General Documentation
*   `DRONE_RE_FINAL_REPORT.md`: Comprehensive final report of the project.
*   `REVERSE_ENGINEERING_NOTES.md`: Raw technical notes and observations.
*   `conversation_gemini.md`: Full conversation history with Gemini CLI.

---

## 🛠 Tools Used
*   **Gemini CLI**: AI-assisted analysis, automation, and documentation.
*   **Ghidra**: Open-source software reverse engineering suite.

---

## 📜 Disclaimer
The information and files in this repository are for research purposes only. Use the firmware or memory dumps at your own risk.
