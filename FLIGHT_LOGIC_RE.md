# Flight Logic Reverse Engineering Report - HW2181 Drone (Updated)

## 1. IMU Sensor (Orientation & Stability)
- **Sensor Chip:** MPU6050 (or compatible).
- **I2C Address:** `0x69` (WHO_AM_I: `0x68`).
- **Data Acquisition:** `IMU_Read` (`0x3b58`) reads 14 bytes from register `0x3B`.

## 2. PID Stabilization Loop (Verified Parameters)
The stabilization logic is located at `0x00000f82`. It uses single-precision floating point math.

### PID Gain Constants
| Parameter | Value | Role |
| :--- | :--- | :--- |
| **P Gain** | 3.0 | Strength of attitude correction (Pitch/Roll). |
| **I Gain** | 0.004 | Drift correction accumulator. |
| **D Gain** | 0.02 | Oscillation dampening. |
| **Output Scale**| 100.0 | Multiplier for PWM duty cycle. |

### Sensor Fusion
- **Filter Type:** Complementary Filter.
- **Weights:** 0.95 (Gyro) / 0.05 (Accel). 
- **Yaw Filter:** 0.9 (Trusts gyro heavily for rotation).

## 3. Telemetry (Drone -> Remote)
Found in `FUN_000024b4`:
- **Packet Type:** `0x05` (Drone Beacon).
- **Format:** `[0x05] [Status] [Channel] [ID_Byte1] [ID_Byte2] [ID_Byte3]`
- **Usage:** Transmitted on Channel 3 during pairing to notify the remote of the drone's presence.

## 4. Motor Control (PWM Generation)
- **T16N0 (0x4000_2000):** Front-Right (CH0), Front-Left (CH2).
- **T16N3 (0x4000_2C00):** Back-Right (CH0), Back-Left (CH2).
- **Duty Cycle:** 0 - 1000.
