# HW2181 SOC Pinout Reference (QFN48) - Complete Master List

This document provides the full 1-48 pin mapping for the **HW2181** 2.4G SOC.

## 1. Complete Pin Mapping

| Pin | Name | Function | Note |
| :--- | :--- | :--- | :--- |
| 1 | VDDRF | RF Power | 2.5V ~ 3.6V RF Supply |
| 2 | XTALP | Crystal In | 12/16/20MHz Crystal |
| 3 | XTALN | Crystal Out | RF Crystal output |
| 4 | VDDPA | RF Power | 1.8V Power Amp Output |
| 5 | RFP | RF P-Side | **Antenna Connection** |
| 6 | RFN | RF N-Side | **Antenna Connection** |
| 7 | VDDIF | RF Internal | 1.8V Analog Power Output |
| 8 | VDDRF | RF Power | 2.5V ~ 3.6V RF Supply |
| 9 | VDDRF | RF Power | 2.5V ~ 3.6V RF Supply |
| 10 | VDD18 | Logic Power | 1.8V Digital Power Output |
| 11 | CE | Chip Enable | High=Active, Low=Reset |
| 12 | PA18 | I2C SCL / PWM | Status LED 1 |
| 13 | PA19 | I2C SDA / PWM | Status LED 2 |
| 14 | PA20 | GPIO / PWM | Expansion |
| 15 | PA23 | GPIO | Expansion |
| 16 | PA24 | SPI1 MOSI | Debug/External SPI |
| 17 | PA25 | SPI1 MISO | Debug/External SPI |
| 18 | PA26 | GPIO / PWM | Expansion |
| 19 | PA27 | GPIO / PWM | Expansion |
| 20 | PA28 | GPIO / PWM | Expansion |
| 21 | PA29 | GPIO / PWM | Expansion |
| 22 | PA30 | UART / SPI | Expansion |
| 23 | PA31 | UART / SPI | Expansion |
| 24 | PB0 | UART1 RX | **Serial Debug RX** |
| 25 | PB1 | UART1 TX | **Serial Debug TX** |
| 26 | PB2 | I2C SCL | Primary Sensor Bus |
| 27 | PB3 | I2C SDA | Primary Sensor Bus |
| 28 | PB4 | GPIO | Expansion |
| 29 | PB5 | ADC 0 | Analog Input |
| 30 | PB6 | ADC 1 | Analog Input |
| 31 | PB7 | ADC 2 | Analog Input |
| 32 | PB8 | ADC 3 / PWM | Analog Input |
| 33 | GND | Ground | Main Digital Ground |
| 34 | PB10 | OSC1I | MCU Crystal In |
| 35 | PB11 | OSC1O | MCU Crystal Out |
| 36 | PB12 | MRSTN | **Hard Reset** (Active Low) |
| 37 | PB13 | Buzzer / PWM | **Beeper Output** |
| 38 | VDD | MCU Power | 2.5V ~ 3.6V Main Supply |
| 39 | PA0 | SWD CLK | **Programming CLK** |
| 40 | PA1 | SWD DATA | **Programming DIO** |
| 41 | PA2 | ADC / PWM | Expansion |
| 42 | PA3 | ADC / PWM | Expansion |
| 43 | PA4 | ADC / PWM | Expansion |
| 44 | PA5 | ADC / PWM | Expansion |
| 45 | PA6 | PWM / ADC | **Motor 1 (Front Right)** |
| 46 | PA7 | PWM / ADC | **Motor 2 (Front Left)** |
| 47 | PA8 | PWM / ADC | **Motor 3 (Back Right)** |
| 48 | PA9 | PWM / ADC | **Motor 4 (Back Left)** |

## 2. Internal MCU-to-RF Connections
These are not package pins, but internal silicon traces:
- **PA11:** RF IRQ (Interrupt)
- **PA12:** SPI0 MISO
- **PA13:** SPI0 MOSI
- **PA14:** SPI0 CSN (Chip Select)
- **PA15:** SPI0 SCK (Clock)
- **PA16:** RF CE (Radio Module Enable)
