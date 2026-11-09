# openBassSpace

Low-frequency stereo field shaper and mono-maker plugin.

![openBassSpace native editor](assets/native-ui.png)

*Captured from the running VST3 editor.*

## Overview

openBassSpace controls the stereo width of low frequencies:
- Frequency: 20 Hz to 500 Hz (4th-order Linkwitz-Riley crossover)
- Bass Width: 0% to 200%
  - 0%: Collapses low frequencies to mono for club playback and mono compatibility
  - 100%: Flat pass-through
  - 200%: Stereo width expansion for spatial bass effects

Frequencies above the crossover are recombined with flat frequency response and zero phase cancellation.

## Formats
- VST3
- Audio Unit (AU)
- Standalone
