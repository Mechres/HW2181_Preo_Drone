# Persistent Storage Specification - HW2181 Drone

This document details how the drone saves its settings (Pairing ID, Calibration) to the internal Flash memory.

## 1. Storage Location
The HW2181 uses the final page of its 36KB Flash for persistent storage.
- **Start Address:** `0x0000_8C00`
- **Active Slot:** `0x0000_8FF8` (Final 8 bytes of Flash).

## 2. Setting Signature Breakdown (Verified)
The following 8-byte structure was found in the firmware dump:
`51 35 50 69 01 CC 00 00`

| Offset | Name | Value (Example) | Description |
| :--- | :--- | :--- | :--- |
| 0 | HEADER | `0x51` | Magic byte indicating valid settings. |
| 1 | CHANNEL | `0x35` | The active flight channel (calculated from ID). |
| 2-3 | CALIB | `0x50 69` | IMU Calibration data or integrity checksum. |
| 4-5 | CONTROLLER_ID | `0x01 CC` | The 16-bit ID of the paired remote control. |
| 6-7 | RESERVED | `0x00 00` | Future use or padding. |

## 3. Pairing Logic Verification
The stored **CHANNEL** byte confirms the frequency hopping formula:
`Stored_Channel = (ID_High + ID_Low & 0x1F) + 0x28`

In the dump:
- ID = `0x01CC`
- Calc: `(0x01 + 0xCC) & 0x1F + 40` = `205 & 31 + 40` = `13 + 40` = **53 (0x35)**.
- **Match:** The stored byte at offset 1 is exactly `0x35`.

## 4. Usage for Developers
To clone a remote control or bypass pairing:
1.  Read the ID from your remote control's packets (Bytes 6-7).
2.  Calculate the channel using the formula above.
3.  Write the 8-byte signature to `0x8FF8` using an SWD programmer (OpenOCD/ST-Link).
4.  The drone will now connect to the remote instantly on boot.
