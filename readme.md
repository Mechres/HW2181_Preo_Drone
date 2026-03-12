# Preo RQ77-14W Reverse Engineering Project

This repository contains the results of a reverse engineering effort on the Preo RQ77-14W drone firmware. The project was facilitated using **Gemini CLI** in conjunction with **Ghidra**.

> **Note:** This project is a personal exploration and not the work of a professional security researcher. The findings are provided as-is for educational and research purposes.

---

## 🚀 Latest Findings Summary
The reverse engineering process is substantially complete. We have successfully mapped the architecture of the **HW2181 SOC (ARM Cortex-M0)**:
*   **Radio Protocol:** Identified the pairing mechanism on Channel 3 and the adaptive frequency hopping algorithm based on the Controller ID.
*   **Flight Logic:** Located the PID stabilization loop and the MPU6050 IMU integration via I2C.
*   **Motor Control:** Mapped the 4 PWM channels to hardware timers T16N0 and T16N3.
*   **Safety Systems:** Decoded the multi-level battery monitoring (ADC) and signal-loss failsafe logic.

---

## 📂 File Manifest

### Firmware & Memory Dumps
*   `fw_full.bin`: The complete firmware dump from the drone.
*   `ram_live.bin`: A live RAM dump captured during operation.

### Specialized Reports (New)
*   [`RADIO_PROTOCOL_RE.md`](RADIO_PROTOCOL_RE.md): Detailed analysis of the 2.4GHz RF communication, pairing, and packet structure.
*   [`FLIGHT_LOGIC_RE.md`](FLIGHT_LOGIC_RE.md): Breakdown of the IMU sensor integration, PID loop, and motor PWM control.
*   [`SAFETY_AND_BATTERY_RE.md`](SAFETY_AND_BATTERY_RE.md): Documentation of ADC-based battery monitoring and failsafe mechanisms.

### General Documentation
*   `DRONE_RE_FINAL_REPORT.md`: A comprehensive final report detailing the findings of the reverse engineering process.
*   `REVERSE_ENGINEERING_NOTES.md`: Raw technical notes and observations.
*   `conversation_gemini.md`: The full conversation log between the user and Gemini CLI.

### Hardware Reference
*   `HW2181_cn.md`: Information regarding the HW2181 chip.
*   `HW2181_datasheet_cn.md`: Detailed datasheet for the HW2181 SOC (Cortex-M0 + 2.4G RF).

---

## 🛠 Tools Used
*   **Gemini CLI**: AI-assisted analysis, automation, and documentation.
*   **Ghidra**: Open-source software reverse engineering suite.

---

## 📜 Disclaimer
The information and files in this repository are for research purposes only. Use the firmware or memory dumps at your own risk.
