# Flight Logic Reverse Engineering Report - HW2181 Drone

## 1. IMU Sensor (Orientation & Stability)
- **Sensor Chip:** MPU6050 (or compatible).
- **I2C Address:** `0x69` (Alternate address).
- **Ghidra Entry Points:**
    - `FUN_00003b58`: **Read_IMU** (Reads 14 bytes from register `0x3B`).
    - `FUN_000038f2`: **Process_IMU** (Calibration, offset subtraction, and signal conditioning).
- **Data Format:** 16-bit signed integers for Accel (X, Y, Z) and Gyro (X, Y, Z).

## 2. Motor Control (PWM Generation)
The drone uses two hardware timers to control 4 motors.
- **Timer 0 (T16N0):** Base `0x4000_2000`
    - Channel 0 (`MAT0`): **Front-Right Motor**
    - Channel 2 (`MAT2`): **Front-Left Motor**
- **Timer 3 (T16N3):** Base `0x4000_2C00`
    - Channel 0 (`MAT0`): **Back-Right Motor**
    - Channel 2 (`MAT2`): **Back-Left Motor**
- **PWM Frequency:** Determined by the timer reload value (stored at `0x1FE0`, value `999` decimal).
- **Duty Cycle:** 0 to 1000.

## 3. Flight Controller (PID Loop)
The main stabilization loop is located within `FUN_00000f82`.
- **Logic:**
    1. Read Remote Control Commands (Throttle, Pitch, Roll, Yaw).
    2. Read IMU Sensors (Gyro/Accel).
    3. Calculate Error (Commanded - Actual).
    4. Apply PID correction.
    5. Update Motor Speeds via `UpdateMotors(FL, FR, BL, BR)`.

## 4. Key Constants (Tuning Parameters)
Floating-point constants found at `0x1348`:
- `0.95`: Likely a **Complementary Filter** weight for Pitch/Roll.
- `100.0`: Scaling factor for motor output.
- `3.0`: Proportional Gain (P) or Tilt Limit.
- `0.9`: Alternative filter weight for Yaw.

## 5. Next Steps
- Identify the **Safety Cut-off** logic (Low battery or Signal loss).
- Map the specific bytes in the 8-byte radio packet to Throttle, Pitch, Roll, and Yaw.
