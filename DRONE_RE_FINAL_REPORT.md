# Final Reverse Engineering Report: Eastsoft HW2181 Drone

## Target Device
- **Chip:** Eastsoft HW2181FHNQ
- **Description:** Integrated 2.4GHz RF + Cortex-M0
- **Application:** Toy-grade Quadcopter with Wi-Fi Camera

## Peripheral Map
| Component | Address | Details |
|-----------|---------|---------|
| PWM Timer 0 | `0x40002000` | Motors 1 & 2 (Offsets 0x30, 0x38) |
| PWM Timer 1 | `0x40002C00` | Motors 3 & 4 (Offsets 0x30, 0x38) |
| I2C0 | `0x40009000` | IMU (0x69) and Barometer (0x76) |
| UART0 | `0x40001C00` | Wi-Fi Module Interface |
| ADC | `0x40001000` | Battery Monitor (Channel 0x0C) |
| RF Block | `0x40008000` | Internal SPI for 2.4GHz Radio |

## Wi-Fi Control Protocol (UART Bridge)
- **Baud Rate:** Likely 115200 or 19200.
- **Packet Length:** 8 Bytes
- **Format:** `0x66 | P | R | T | Y | F | CS | 0x99`
- **Checksum:** XOR of data bytes 1-5.

## Sensor Config
- **IMU:** InvenSense MPU series (WHO_AM_I = 0x68, Address = 0x69).
- **Baro:** BMP280 (Address = 0x76).

## Flight Constants
- **PWM Max:** 1000
- **Flight Cap:** 880 (88% power)
- **PID Loop Frequency:** Likely 250Hz or 500Hz.
