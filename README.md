# SpeScan-PV

**SpeScan-PV** is an experimental optoelectronic prototype for measuring the spectral response of photovoltaic (PV) samples. The system selects wavelength bands by rotating a diffraction grating, measures the electrical response of the PV sample, and streams voltage, current, and power data in real time.

## Project objective

The prototype is designed to help determine the wavelength range in which a photovoltaic sample produces its strongest electrical response. It combines an optical scanning module, an ESP32 controller, an INA219 current/voltage sensor, servo-controlled diffraction grating, and an optional Nextion display.

## Repository contents

- `firmware/SpeScanPV_ESP32/SpeScanPV_ESP32.ino` — working ESP32 firmware
- `docs/WIRING.md` — wiring and power notes
- `docs/CALIBRATION.md` — wavelength calibration procedure
- `data/sample_output.csv` — example CSV format produced by the firmware

## Hardware used in the prototype

- ESP32 / ESP32-PICO-DevKitM-2
- INA219 current and voltage sensor
- DS3218 digital servo motor
- Quartz tungsten-halogen light source
- Collimator lens
- Diffraction grating
- Focusing lens
- PV sample
- Optional Nextion display

## Firmware functions

The firmware can:

- scan from 400 nm to 1100 nm in configurable steps;
- rotate the diffraction grating using a calibrated wavelength-to-angle table;
- average multiple INA219 measurements at each spectral point;
- calculate and output load voltage, current, and power;
- stream measurement results as CSV over USB serial;
- display wavelength, voltage, current, and power on a Nextion HMI;
- accept serial commands for manual and automatic measurements.

## Serial commands

At `115200 baud`:

```text
START       Start full spectral scan
STOP        Request scan stop
MEASURE     Measure at 550 nm
GOTO:650    Move to 650 nm and measure
STATUS      Show hardware status and command list
```

## Required Arduino libraries

Install using Arduino Library Manager:

1. **Adafruit INA219**
2. **ESP32Servo**

The ESP32 board package must also be installed in Arduino IDE.

## Important scientific note

The wavelength values are only physically meaningful after optical calibration. The included wavelength-to-servo table contains initial placeholder anchors for software testing. Before reporting experimental results, replace these values with calibration points measured using a reference spectrometer, monochromator, or known spectral emission lines.

## Safety

The 1000 W tungsten-halogen lamp and its mains/transformer circuit operate at hazardous voltage, current, and temperature. The lamp power circuit must be electrically isolated from the ESP32 electronics, enclosed, fused, grounded where required, and assembled by a qualified person. Never power the servo motor directly from the ESP32 5 V/3.3 V rail.

## Prototype status

Current status: **functional firmware prototype / hardware integration stage**. Optical calibration and measurement uncertainty characterization are required before the device can be treated as a calibrated scientific instrument.

## License

MIT License. See `LICENSE`.
