# Drone Reverse Engineering: Developer Cheat Sheet

This file is a quick reference for the Preo RQ77-14W / HW2181 firmware. Use these addresses in Ghidra to understand or modify specific drone behaviors.

## 1. Flight Control & PID
- **Main Loop:** `0x00000eb8` (The heart of the flight controller)
- **PID Routine:** `0x00000f82` (Stabilization logic)
- **IMU Processor:** `0x000038f2` (Filters accelerometer/gyro data)
- **Motor Speed Update:** `0x00001f30` (Sets PWM values)

## 2. Radio & Protocol
- **Packet Decoder:** `0x00003c6e` (Converts radio bytes to commands)
- **Hopping Logic:** Found in `Radio_Packet_Handler` (`0x00001cd2`)
- **RF Register Write:** `0x0000192c`
- **RF Buffer Read:** `0x000019dc`

## 3. Hardware Signaling
- **Buzzer Driver:** `0x00001e0c` (Short beeps/long tones)
- **LED Control:** `0x0000201c` (Blink patterns)
- **LED Hardware Turn ON:** `0x00002062` (Sets Pin 12/13 High)
- **ESC UART Init:** `0x00001a1c` (Sets up UART1 for motor controller)

## 4. Critical Constants
- **Radio ID / Pair Signature:** Offset `0x8FF8` in Flash.
- **Baud Rate (UART1):** 19200.
- **I2C IMU Address:** 0x69 (MPU6050).
- **PWM Limit:** 1000 (Maximum allowed duty cycle).
