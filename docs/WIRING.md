# Wiring Guide

## ESP32 connections

| Module | Signal | ESP32 pin |
|---|---|---:|
| INA219 | SDA | GPIO 21 |
| INA219 | SCL | GPIO 22 |
| DS3218 servo | PWM signal | GPIO 25 |
| Nextion | ESP32 RX | GPIO 32 |
| Nextion | ESP32 TX | GPIO 33 |

## INA219

Connect INA219 in series with the PV sample/load according to the sensor board documentation. The I2C address is assumed to be the default `0x40`.

## Servo power

The DS3218 servo must use a separate regulated supply appropriate for the servo. Connect the servo supply ground to ESP32 ground so that the PWM signal has a common reference.

## Nextion display

Use UART at 9600 baud by default. The firmware sends values to these text components:

- `tWave`
- `tVolt`
- `tCurr`
- `tPower`
- `tStatus`

If your HMI project uses different component names, change them in the firmware.

## 1000 W lamp safety

The lamp power circuit is NOT controlled directly from the ESP32. Keep mains/high-power wiring physically and electrically separated from low-voltage control electronics. Use a rated enclosure, fuse, switchgear, thermal protection, and proper grounding.
