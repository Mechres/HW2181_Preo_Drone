# Drone Firmware Reverse Engineering Notes

## Hardware Overview
- **Microcontroller (SoC):** Eastsoft HW2181FHNQ
- **Core:** ARM Cortex-M0 @ 48MHz
- **Memory:** 36KB Flash, 64KB SRAM (Based on typical variants)
- **Wireless:** Integrated 2.4GHz GFSK transceiver (250Kbps/1Mbps) 
  - *Note: This is a proprietary 2.4GHz RF radio (like an NRF24L01), not a standard 802.11 Wi-Fi chip.*

## Memory Map & Peripherals
- **Timer/PWM (Motors):** `0x40002000` and `0x40002C00` (Duty cycle set at offsets `+0x30` and `+0x38`)
- **I2C0:** `0x40009000` - Used to communicate with the BMP280 Barometer (Device ID `0x76`) for altitude hold.
- **ADC (Battery Monitor):** Reads from channel `0x0C`. LVC (Low Voltage Cutoff) thresholds dynamically adjust duty cycles.
- **Internal RF Block:** Initialized via registers at `0x40008000` and configured via a custom SPI-like interface.

## Firmware Architecture
- **Main Task Loop:** `FUN_00000eb8`. This loop continuously fetches sensor data, polls the radio buffers, runs the PID controller, and updates motor speeds.
- **Flight Controller / Mixer:** `FUN_000040aa`. Implements a software-emulated floating-point PID controller. Caps motor duty cycles at `88%` (`880/1000`) to ensure there is always headroom for gyro stabilization.
- **Motor PWM Updater:** `UpdateMotors` (`FUN_00001f30`). Takes 4 integer inputs (0-1000) and maps them directly to the hardware timer registers.
- **Battery Management:** `FUN_000021c8`. Smooths ADC readings to monitor voltage dips.

## RF / Radio Protocol (Native 2.4GHz Remote)
- **RF Configuration:** `FUN_00001a72` writes configuration arrays to the internal WRC (Wireless RF Controller).
- **Sync Word:** `55 AA` (Characteristic of Bayang or similar Chinese toy drone protocols).
- **Packet Structure:** Processed in `FUN_00002582`. The payload decodes sticks using the formula: `Value = (ByteValue * 4) - 400`.
  - `Byte [0x0A]`: Pitch
  - `Byte [0x0B]`: Roll
  - `Byte [0x0C]`: Yaw
  - `Byte [0x0D]`: Throttle
  - `Byte [0x0E] & [0x0F]`: Bitmasks for Flips, Arming, and Flight Modes.

## Wi-Fi Control Hypothesis
Since the HW2181 does not possess native 802.11 Wi-Fi, drones that feature Wi-Fi control (via a smartphone app) use a separate **Camera/Wi-Fi Module** (usually containing an ESP8285, RTL8189, or BL8028). 
This external module connects to the HW2181 flight controller via a **UART (Serial) connection** (TX/RX pins). When you use the phone app, the Wi-Fi module parses the app's TCP/UDP packets and translates them into a stream of serial bytes sent directly to the HW2181.