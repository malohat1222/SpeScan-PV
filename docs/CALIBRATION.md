# Spectral Calibration

The servo angle cannot be converted to wavelength accurately from software alone because the relationship depends on the diffraction grating, incidence angle, optical geometry, slit position, and mechanical mounting.

## Recommended procedure

1. Assemble and mechanically align the light source, collimator, diffraction grating, focusing optics, and output slit/PV detector.
2. Use a reference spectrometer, monochromator, or known narrow spectral lines.
3. Move the grating slowly and record the servo angle at each known wavelength.
4. Collect at least 5–10 calibration points across the intended range.
5. Replace the `calibration[]` table in the firmware with the measured wavelength-angle pairs.
6. Re-run intermediate wavelengths and calculate calibration residuals.
7. Document repeatability, wavelength error, current/voltage uncertainty, and environmental conditions.

## Example table format

```cpp
CalibrationPoint calibration[] = {
  {405.0f, 54.8f},
  {532.0f, 77.1f},
  {650.0f, 96.4f},
  {850.0f, 129.3f},
  {1064.0f, 164.7f}
};
```

The values above are formatting examples only and must not be reported as measured calibration data.
