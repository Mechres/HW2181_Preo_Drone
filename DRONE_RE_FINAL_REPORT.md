# Preo RQ77-14W Reverse Engineering Final Report

## 1. Executive Summary
This report documents the architectural mapping and protocol decoding of the Preo RQ77-14W drone, powered by the **HW2181 2.4G SOC** (ARM Cortex-M0). All critical systems including flight control, radio communication, and safety failsafes have been analyzed.

---

## 2. Hardware Architecture
- **CPU:** ARM Cortex-M0 (32-bit, Thumb instruction set).
- **RAM:** 8KB (0x2000_0000 - 0x2000_1FFF).
- **Flash:** 36KB (0x0000_0000 - 0x0000_8FFF).
- **Sensors:** MPU6050 IMU (I2C Address: `0x69`, WHO_AM_I: `0x68`).
- **RF:** Integrated 2.4GHz Transceiver (Internal SPI0).

---

## 3. Protocol & Control Decoded (Verified)

### 3.1 Radio Control Packet (8 Bytes)
Located at `0x200000E8`.
- **Mapping:** Type (0), Pitch (1), Roll (2), Throttle (3), Yaw (4), Buttons (5), ID (6-7).
- **Transformation Formula:** `Internal_Value = (Raw_Byte * 4) - 400`
    - *Correction:* The previously noted `-512` was for a different firmware version. This specific dump uses `-400` (verified in `FUN_00002582`).
    - *Range:* -400 to +620.

### 3.2 Frequency Hopping
- **Pairing Channel:** 0x03 (2403 MHz).
- **Operational Channel:** `(ID_High + ID_Low & 0x1F) + 0x28`.

---

## 4. Flight Control System

### 4.1 Motor PWM Mapping
The drone uses 16-bit timers for precise motor control:
- **T16N0 (0x4000_2000):**
    - Channel 0: Front-Right Motor.
    - Channel 2: Front-Left Motor.
- **T16N3 (0x4000_2C00):**
    - Channel 0: Back-Right Motor.
    - Channel 2: Back-Left Motor.
- **Reload Value:** 1000 (Frequency = SystemClock / 1000).

### 4.2 PID Stabilization
The PID loop runs in `Main_Flight_Loop` (`0x00000eb8`).
- **Gains:**
    - **P (Proportional):** 3.0 (Scaling factor for error).
    - **I (Integral):** Accumulator found at `20000048`.
    - **Complementary Filter:** 0.95 (High-pass gyro) / 0.05 (Low-pass accel).

---

## 5. Safety & Failsafes
- **Battery Monitoring:** ADC Channel 12. Warning at `0x0924`, Critical at Stage 2 thrust reduction.
- **Signal Loss:** If no packet is received for > 500ms, `UpdateMotors(0,0,0,0)` is called.

---

## 6. Developer Interface
- **UART Debug:** PB0 (RX) / PB1 (TX).
- **Baud Rate:** 19200 (Calculated from `UART_BRR` divisor).
- **SWD Programming:** PA0 (CLK) / PA1 (DIO).
