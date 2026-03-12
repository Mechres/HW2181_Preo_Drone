# Radio Packet Specification - HW2181 Drone

This document defines the 8-byte data packet used for remote control communication.

## 1. Packet Structure
Incoming packets from the remote control are stored in a buffer (RAM: `0x200000E8`).

| Index | Name | Range | Center | Description |
| :--- | :--- | :--- | :--- | :--- |
| 0 | TYPE | 0-255 | N/A | Packet type (0x01=Pairing, 0x02=Data) |
| 1 | PITCH | 0-255 | 128 | Forward/Backward tilt |
| 2 | ROLL | 0-255 | 128 | Left/Right tilt |
| 3 | THROTTLE| 0-255 | 0 | Vertical lift (Thrust) |
| 4 | YAW | 0-255 | 128 | Clockwise/Counter-clockwise rotation |
| 5 | BUTTONS | Bitmask| N/A | Feature toggles (Flip, Trim, etc.) |
| 6 | ID_HI | 0-255 | N/A | Controller ID High Byte |
| 7 | ID_LO | 0-255 | N/A | Controller ID Low Byte |

## 2. Command Transformation
The firmware scales the 8-bit stick values into 16-bit signed integers for the PID loop:
- **Formula:** `Internal_Value = (Raw_Byte * 4) - 400`
- **Resulting Range:** `-400` to `+620`
- **Center (stick neutral @ 128):** `(128 * 4) - 400 = 112`
- **Verified in:** `FUN_00002582`

## 3. Button Bitmask (Byte 5)
- **Bit 0 (0x01):** **Auto-Takeoff/Landing**. Sets flight state to 0x03.
- **Bit 2 (0x04):** **IMU Calibration**. Holding this bit high while sticks are centered triggers the zero-offset routine.
- **Bit 3 (0x08):** **Headless Mode**. Toggles orientation-independent control.

## 4. Special Maneuvers (Flip)
When a "Flip" command is detected (combination of Byte 5 and high deflection on Pitch/Roll), the drone:
1.  Disables the Z-axis (Throttle) PID loop.
2.  Applies maximum torque to the target axis.
3.  Waits for the IMU to report a ~360 degree rotation.
4.  Re-enables stabilization.
