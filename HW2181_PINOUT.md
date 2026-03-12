# HW2181 SOC Pinout Reference (QFN48)

This document maps the physical and internal pins of the **HW2181** 2.4G SOC used in the drone. This is critical for locating hardware test points (UART, SWD, PWM) on the PCB.

## 1. External Pin Mapping (QFN48 Package)

| Pin | Name | Primary Function | Secondary / Notes |
| :--- | :--- | :--- | :--- |
| **1** | VDDRF | RF Power | 2.5V ~ 3.6V input |
| **2** | XTALP | Crystal In | 12/16/20MHz RF Crystal |
| **3** | XTALN | Crystal Out | RF Crystal output |
| **5** | RFP | RF P-Side | **Antenna Connection** |
| **6** | RFN | RF N-Side | **Antenna Connection** |
| **11** | CE | Chip Enable | High = Active, Low = All-Chip Reset |
| **12** | PA18 | I2C SCL / PWM | I2C Clock / LED Control |
| **13** | PA19 | I2C SDA / PWM | I2C Data / LED Control |
| **16** | PA24 | SPI1 MOSI | External SPI Data Out |
| **17** | PA25 | SPI1 MISO | External SPI Data In |
| **24** | PB0 | UART1 RX | **Serial Debug RX** / PWM |
| **25** | PB1 | UART1 TX | **Serial Debug TX** / PWM |
| **33** | GND | Ground | Main Digital Ground |
| **34** | PB10 | OSC1I | MCU External Crystal In |
| **35** | PB11 | OSC1O | MCU External Crystal Out |
| **36** | PB12 | MRSTN | **Hardware Reset Pin** (Low-active) |
| **38** | VDD | MCU Power | 2.2V ~ 3.6V Processor Power |
| **39** | PA0 | SWD CLK | **Debug Interface (CLK)** |
| **40** | PA1 | SWD DATA | **Debug Interface (DIO)** |
| **45** | PA6 | ADC / PWM | Motor Control (Front Right?) |
| **46** | PA7 | ADC / PWM | Motor Control (Front Left?) |
| **47** | PA8 | ADC / PWM | Motor Control (Back Right?) |
| **48** | PA9 | ADC / PWM | Motor Control (Back Left?) |

## 2. Internal MCU-to-RF Interface
The following connections are **internal** to the silicon and are not exposed on the package pins. The MCU controls the built-in 2.4G radio via these dedicated lines:

| MCU Pin | RF Pin | Function | Description |
| :--- | :--- | :--- | :--- |
| **PA14** | CSN | SPI0 CS | RF Chip Select |
| **PA15** | SCK | SPI0 SCK | RF SPI Clock |
| **PA13** | MOSI | SPI0 MOSI | Data MCU -> Radio |
| **PA12** | MISO | SPI0 MISO | Data Radio -> MCU |
| **PA11** | IRQ | RF IRQ | Radio Interrupt Signal |
| **PA16** | CE | RF CE | Radio Module Enable |

## 3. Recommended Test Points for RE
If you are probing the physical PCB, look for these first:
- **Programming/Debug:** Look for pads connected to **Pins 39 & 40** (SWD). This is how you would reflash the firmware.
- **Serial Console:** Probing **Pins 24 & 25** during startup may reveal debug logs (likely at 115200 baud).
- **Motor Control:** The signals to the motor MOSFETs will originate from **Pins 45-48**.
- **Radio Signal:** The trace from **Pins 5 & 6** will lead to the 2.4GHz antenna.
