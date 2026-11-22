# Scientific calibration and validation plan

**Status:** Test plan, not evidence that any test was completed.

## 1. Optical wavelength calibration

- Verify commanded grating position against known spectral lines or an independent reference spectrometer.
- Fit wavelength-to-actuator mapping and retain residuals and calibration metadata.
- For two-edge operation, independently calibrate `λ1` and `λ2` against slit-edge positions.
- Measure spectral band shape, stray light, edge roll-off and potential overlapping orders.

## 2. Optical power calibration

- Characterize lamp output at each optical configuration and at the actual sample plane.
- Use a calibrated reference photodiode or optical-power measurement system with an appropriate spectral correction.
- Account for spectral irradiance, spatial illumination uniformity, optical losses and background signal.
- Report the measurement conditions and optical power uncertainty.

## 3. Electrical and thermal calibration

- Verify current and voltage readings against reference instruments over the intended dynamic range.
- Confirm sensor bandwidth, resolution, input range, thermal drift and grounding.
- Calibrate the cell temperature sensor and record sensor attachment method and ambient conditions.
- Do not claim full I–V or MPP measurements from a fixed-load INA219 channel alone; use a tested programmable load or equivalent controlled sweep.

## 4. Experiment sequence

1. Dark/background measurement with lamp blocked, document offset.
2. Reference optical-power measurement for selected spectral band.
3. Si cell: repeated current, voltage, power, temperature and (when equipped) I–V tests.
4. Ge cell: repeat only within calibrated, optically supported range; consider necessary near-IR extension.
5. GaAs cell: repeat within material-appropriate and calibrated bands.
6. Compare samples only under defined matching conditions; separate sample area, optical power, temperature and load effects.
7. Compare any future candidate filter with unfiltered controls using net electrical output and temperature over time.

## 5. Equations and reporting

At a defined DC operating point: `P_el = U × I`.

For narrowband measurement under appropriate conditions: `R(λ) = I_ph(λ) / P_opt(λ)`.

For a finite band under linear response: `I_ph = integral_λ1^λ2 R(λ) P_λ(λ) dλ`.

Include type A repeatability and type B sources (reference meter, optical-power calibration, wavelength calibration, slit width, temperature). Report combined uncertainty with model assumptions; set acceptance limits **before** evaluating data.

## 6. Transparent results policy

- Label simulated outputs as **simulation**, not measurements.
- Label generated sample CSV rows as **illustrative/test data**.
- Do not claim optimal wavelengths, efficiency improvement, cooling effect, response speed or validated uncertainty until supported by experimental data and a documented comparison.
- Keep scripts and raw measurement files with units, timestamps, sample IDs, configuration, reference instrument IDs and calibration version.
