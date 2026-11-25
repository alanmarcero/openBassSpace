# Changelog

## v2.7.3

- Refines preset browsing, lock behavior, and editor details across the UI polish passes.
- Aligns the JUCE 8 license and hardens CI.

## v2.7.2

- **Parameter Locking:** Add per-parameter locking across every plugin parameter, persisted in session state and preserved across preset loads and randomization.

## v2.7.1

- **UI & Suite Fixes:** UI polish, unit formatting and typed-value entry, inactive control dimming, per-slot A/B undo, inline preset confirms, peak-hold meters, uniform scaling, and screen-reader titles.

## v2.7.0 — 2026-11-20

2.7.0 upgrades the mastering bass-shaping DSP and adds crossover slope and elliptical controls:

- Selectable 24 dB/oct LR4 or 48 dB/oct LR8 TPT crossover with continuous per-sample cutoff smoothing.
- Elliptical Mid mode high-passing only the Side channel while keeping Mid phase-linear.
- Frequency-progressive bass width (keeping sub-35 Hz centered) and energy-preserving low-band mono sum makeup.
- Sub drive with octave-down f/2 sub-harmonic synthesis, ADAA-1 saturation, and phase-matched post-LPF.
- Symmetrical TPT upper-band tilt shelf smoothed in log-gain dB, sample-rate-invariant 4-stage Schroeder/Velvet stereoizer, and true-peak-safe M/S ceiling guard at -0.1 dBFS.
- Added Slope and Elliptical Mid buttons in the editor and refreshed the preview screenshot.
