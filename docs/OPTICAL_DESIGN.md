# SpeScan optical system — proposed design

**Status:** Engineering concept; not an experimental performance specification.

## Objective

Select a finite wavelength band and irradiate the same designated photovoltaic sample area while measuring electrical output and temperature. The planned design targets two independently selectable wavelength limits, `λ1` and `λ2`.

## Proposed optical train

`12 V / 100 W quartz tungsten-halogen source (+ reflector and cooling) → collimating lens → reflective diffraction grating (600 lines/mm) → steering mirror → two independently controlled slit edges → field lens → PV sample`.

The actual location and functions of each optic depend on optical design; the instrument needs a spectral image at the slit plane, a stable sample plane, and suppression of stray orders. Lens and mirror choices must be validated by ray tracing and experiment.

### Diffraction grating

For a reflective grating, an often-used signed-angle form is:

`m λ = d (sin α + sin β)`

Here `m` is diffraction order, `d` is grating period, and `α`, `β` are angles relative to the grating normal under the adopted sign convention. For `N = 600 lines/mm`, `d = 1/600 mm = 1666.7 nm`.

For an **illustrative normal-incidence** first-order ray at 660 nm (`α = 0°`, `m = 1`), `β ≈ arcsin(660 / 1666.7) ≈ 23.3°`. This is **not** the angle of the actual designed apparatus without its true geometry.

### Spectral band

- Lower edge `λ1` and upper edge `λ2` are independent **planned** settings.
- Bandwidth `Δλ = λ2 − λ1`.
- Turning a grating selects a central wavelength and is not enough to prove independent edge control.
- The slit-edge-to-wavelength mapping must be calibrated and may be nonlinear.
- Light at different wavelengths must be mapped and measured at the sample; the actual footprint may vary due to aberrations and geometry.

### Sources of error / additional optics

- Grating efficiency and polarization dependence.
- Stray light and overlapping diffraction orders (an order-sorting filter may be required).
- Lens/coating transmission, sample illumination uniformity, thermal drift.
- Servo backlash, encoder resolution, slit position repeatability and grating alignment.
- Near-infrared optical compatibility when extending toward Ge-responsive wavelengths.

## Material specificity

Material band gaps suggest approximate long-wavelength thresholds of Si ~1.1 μm, Ge ~1.9 μm and GaAs ~0.87 μm. These numbers are **not** a substitute for actual measured spectral response. The available 400–1100 nm firmware scan cannot by itself characterize the full Ge near-infrared range.

## Intended future selective filters

The ultimate research goal is a filter whose spectral transmission `T_filter(λ)` is optimized for **net electrical benefit** and thermal management. Reflection of some absorbed radiation may reduce heating, but indiscriminately blocking all infrared radiation can discard electrically useful photons (especially for Si and Ge). The filter must also avoid trapping its own heat near the PV cell.

No completed filter or verified efficiency gain is claimed.
