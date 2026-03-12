# Radio Emulation & Arduino NRF24L01+ Notes

This document summarizes the technical challenges and findings for developers attempting to control the HW2181-based drone using standard NRF24L01+ hardware.

## 1. The "XN297" Barrier
The HW2181 integrated radio is not a direct register-clone of the NRF24L01+. It follows the **XN297L** architecture, which introduces several physical layer incompatibilities with standard NRF24 modules.

### A. Packet Control Field (PCF)
- **NRF24L01+:** Inserts a 9-bit field between the address and the payload (contains payload length, PID, and NO_ACK bit).
- **HW2181/XN297:** Does **not** use this field. It expects the payload to follow the address immediately.
- **Impact:** An NRF24L01+ in standard mode cannot talk to an HW2181. You must use "No-ACK" mode or a library that emulates the XN297 packet structure (like the "Multi-Protocol" module code).

### B. Bit Order (Endianness)
- **NRF24L01+:** Payloads are typically sent LSB-first.
- **HW2181:** Expects bits within each byte to be **MSB-first** (reversed).
- **Solution:** Every byte of the address and payload must be bit-reversed before calling `radio.write()`.

## 2. Verified Protocol Details (From Firmware)

### Payload Structure
The drone expects exactly 8 bytes. The first byte is used as a length/header internally.
- **Byte 0:** `0x07` (Length indicator).
- **Bytes 1-7:** Data (Type, Sticks, Buttons, ID).

### Scrambling (Whitening)
- **Register:** `0x23` (`MISC0`) controls the scrambling seed.
- **Finding:** The firmware initializes this to `0x0300`, which sets the **Scramble Seed to 0**.
- **Status:** Whitening is likely **OFF** or using a default 0-seed, which simplifies emulation if the PCF barrier is overcome.

### CRC Calculation
- **Type:** CRC16 (`x^16 + x^12 + x^5 + 1`).
- **Initial Value:** Set via `MISC1` register (`0x29`). The firmware uses the hardware CRC engine of the SOC.

## 3. Hardware Recommendations
If using an Arduino, a standard NRF24L01+ is difficult to use due to the PCF mismatch. For success, consider:
1.  **XN297L Breakout Board:** Using the actual chip found in the drone or remotes.
2.  **Multi-Protocol Module (JP4IN1 / IRangeX):** These have dedicated XN297 emulation routines already implemented.
3.  **Power Stability:** Always use a **10uF - 100uF capacitor** across the NRF24's 3.3V and GND pins. These modules fail silently if the power rail dips during transmission.

## 4. Final Firmware Mapping
- **Pairing Channel:** `0x03`
- **Hopping Formula:** `((ID_High + ID_Low) & 0x1F) + 0x28`
- **Stick Scaling:** `(Raw_Byte * 4) - 400`
