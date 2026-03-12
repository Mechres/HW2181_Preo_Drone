# Safety & Battery Monitoring Reverse Engineering Report - HW2181 Drone

## 1. Battery Voltage Monitoring
The drone uses the internal 12-bit ADC of the `HW2181` to monitor its battery (likely a 1S LiPo).

- **ADC Peripheral Base:** `0x4000_1000`
- **Core ADC Function:** `FUN_0000218a` (Reads raw 12-bit values from ADC Channel 12).
- **Update Frequency:** Every 10 iterations of the main loop (via `FUN_000021c8`).

## 2. Low Battery Thresholds
The drone uses a multi-level alarm system based on the raw ADC value (0-4095).
- **Full Battery (~4.2V):** ADC values around `0x0C7F` (3199 decimal).
- **Warning Level 1:** Triggers when ADC value drops below `0x0924`.
    - **Action:** Likely flashes LEDs or sends a telemetry warning.
- **Critical Level 2 (Auto-Landing/Shutdown):**
    - Triggers at even lower thresholds (offsets from `0x0924`).
    - **Level A:** Sets power scaling to `0xB4` (180/1000?).
    - **Level B:** Sets power scaling to `0xA0` (160/1000?).
    - **Final Failsafe:** Drops to `100/1000` power to force a landing.

## 3. Signal Loss (Failsafe)
- **Detection Logic:** Located in `FUN_00001cd2`.
- **Mechanism:**
    - The drone maintains a "Packet Received" flag at `20000010`.
    - If no valid packet is received, the drone checks a "Signal Loss" counter.
    - **Action:** If the counter exceeds a threshold, `UpdateMotors(0, 0, 0, 0)` is called to stop the motors.

## 4. Hardware Safety
- **Write Protection:** The drone rigorously uses the `SCU_PROT` register (`0x4000_0000`) to lock/unlock system-critical settings (Clock, Reset, Power) before and after making changes.
- **Fault Flags:** `SCU_FAULTFLAG` (`0x4000_000C`) is monitored to detect CPU or Bus errors.

## 5. Summary of System Constants
- **ADC Magic Key:** `0x55AA6996` (Used to unlock the SCU for ADC config).
- **Battery Reference:** `0x0FFF` (4095) is used for scaling the 12-bit result.
