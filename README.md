# openBassSpace 2.3

Low-frequency stereo field shaper with independent upper-band imaging.

![openBassSpace native editor](assets/native-ui.png)

*Captured from the running VST3 editor.*

## Overview

openBassSpace separates bass width from the upper stereo field at a 4th-order Linkwitz-Riley crossover:
- **Crossover:** 20 Hz to 500 Hz.
- **Bass width:** 0% to 200% below the crossover.
  - 0%: Collapses low frequencies to mono for club playback and mono compatibility
  - 100%: Flat pass-through
  - 200%: Stereo width expansion for spatial bass effects
- **Upper width:** 0% to 200% above the crossover, independently of the bass.
- **Stereoize:** 0% to 100% adds a decorrelated side signal above the crossover. Its side signal cancels in mono, leaving the bass centered.
- **Correlation:** A live output meter reports left/right correlation from -1 (opposite polarity) to +1 (mono/identical).

The two bands recombine with flat magnitude response. Output trim remains automatable from -24 dB to +12 dB.

Commercial feature research and implementation notes are in [docs/COMMERCIAL_FEATURE_RESEARCH.md](docs/COMMERCIAL_FEATURE_RESEARCH.md).

## Formats
- VST3
- Audio Unit (AU)
- Standalone
