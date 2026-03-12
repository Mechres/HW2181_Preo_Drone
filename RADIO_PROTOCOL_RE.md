# Radio Protocol Reverse Engineering Report - HW2181 Drone

## 1. Hardware Interface
- **MCU to RF Interface:** Internal SPI0
- **SPI0 Base Address:** `0x4000_8000`
- **Ghidra Entry Points:**
    - `FUN_0000192c`: RF Write Register (2-byte)
    - `FUN_00001962`: RF Read Register (2-byte)
    - `FUN_000019a2`: RF Write Buffer (Packet TX)
    - `FUN_000019dc`: RF Read Buffer (Packet RX)

## 2. RF Transceiver Initialization
The initialization sequence is located at offset `0x71A0` in the firmware.
- **Base Frequency:** 2.4GHz ISM Band.
- **Initial Pairing Channel:** `0x03` (2403 MHz).

## 3. Communication Protocol
The protocol uses a simple state machine for pairing and active flight.

### Pairing Phase (State 1)
1. Drone listens on Channel `0x03`.
2. Remote control sends a pairing packet containing its unique Controller ID (2 bytes).
3. Drone receives the ID and stores it.

### Active Flight Phase (State 2)
The drone switches to a "Hopping" or "Fixed" operational channel calculated from the Controller ID:
- **Channel Calculation:** `(ID_Byte1 + ID_Byte2 & 0x1F) + 0x28`
- The `+ 0x28` (40 decimal) offset moves the communication to the higher end of the 2.4GHz band (2440 MHz +) to avoid WiFi interference.

## 4. Packet Structure (Incoming Commands)
Based on `FUN_00001cd2`:
- **Length:** 8 Bytes.
- **Byte 0:** Likely Packet Type / Length.
- **Bytes 1-2:** Controller ID (for verification).
- **Bytes 3-7:** Control Data (Throttle, Pitch, Roll, Yaw).

## 5. Next Steps
- Identify the exact mapping of Throttle/Pitch/Roll/Yaw in Bytes 3-7.
- Analyze the `UpdateMotors` function to see how these bytes are converted to PWM signals.
