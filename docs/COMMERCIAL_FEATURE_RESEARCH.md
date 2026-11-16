# Commercial feature research

Research checked 2026-11-16 against primary vendor material.

## Reference: iZotope Ozone Imager

- [Ozone Imager product page](https://www.izotope.com/products/ozone-imager) describes stereo width control, Stereoize modes for mono sources, and vectorscope visualization.
- [Ozone 8 Imager manual](https://downloads.izotope.com/docs/ozone8/imager/index.html) documents per-band width controls, Stereoize, linked bands, and correlation meters.

## Gaps and changes in openBassSpace

The previous processor shaped only the low band and passed the upper band through. It had no mono-source widening or live phase-correlation readout. Version 2.3 adds three related controls:

1. **Independent upper-band width.** The crossover now feeds separate bass and upper-band width controls; the upper control defaults to 100% so previous sessions retain their prior upper-band sound.
2. **Mono-compatible Stereoize.** A first-order all-pass decorrelator adds a controlled side signal above the crossover. The center signal and bass remain unchanged in the mono fold.
3. **Correlation meter.** The editor displays output L/R correlation from -1 to +1 and marks negative correlation in gold.

The implementation keeps the two-band low-frequency focus of openBassSpace. It does not add Ozone's four independently adjustable bands, vectorscope display, Stereoize mode selection, or linked-band control.

## Verification

`StereoFieldShaperTests` covers upper-band narrowing, mono-fold stability under Stereoize, and correlation edge cases. The `native_editor` CTest covers automatable parameter state, rendered high-frequency width/stereoize, correlation reporting, the editor labels, and screenshot capture.
