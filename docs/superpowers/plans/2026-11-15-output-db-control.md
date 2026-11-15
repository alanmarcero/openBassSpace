# openBassSpace dB Output Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use $executing-plans to implement this plan task-by-task.

**Goal:** Add a saved, automatable −24 to +12 dB master output knob to openBassSpace, defaulting to 0 dB.

**Architecture:** Add an APVTS output parameter, smooth its linear gain after the stereo field shaper, and expose it as a third rotary control. Preserve the existing frequency and width controls.

**Tech Stack:** C++20, JUCE 8.0.9, CMake, the existing native plugin-host check executable.

**Spec:** `audio-plugin-output-db-controls-design-2026-11-15.md`

## Global Constraints

- Output range is −24 to +12 dB, with a 0 dB default.
- Keep the current plugin identity and parameter IDs; add only the requested output parameter.
- Use the project's existing JUCE/CMake dependencies and preserve mono and stereo processing.
- Bump the project version from 2.1.1 to 2.2.0.

## Review Focus

- 0 dB must leave output at unity; −6 dB must scale output by about 0.501 — test in the host-check executable.
- Parameter changes must smooth instead of creating a click — compare adjacent output samples during a parameter step.
- Saved state must restore the output dB value — save and restore through the processor state API.
- A legacy state without the new parameter must retain a 0 dB default — restore a state captured before adding the parameter.
- Mono processing and zero-channel safety must continue to work — keep the current host-check cases.

---

### Task 1: Add automatable dB output control

**Files:**
- Modify: `Tests/PluginHostChecks.cpp`
- Modify: `Source/Plugin/Parameters.h`
- Modify: `Source/Plugin/Parameters.cpp`
- Modify: `Source/Plugin/PluginProcessor.h`
- Modify: `Source/Plugin/PluginProcessor.cpp`
- Modify: `Source/Plugin/PluginEditor.h`
- Modify: `Source/Plugin/PluginEditor.cpp`
- Modify: `CMakeLists.txt`
- Modify: `README.md`

**Interfaces:**
- Produces: `bass_space::params::kOutputDb`, the APVTS ID `output_db`, bounded from −24 to +12 dB with 0 dB default.

- [x] **Step 1: Write the failing host checks** for the new parameter's default, state recall, and −6 dB output ratio; include a parameter-step click check.
- [x] **Step 2: Run the host-check executable** and confirm it fails because `output_db` is missing.
- [x] **Step 3: Add the parameter, smooth its gain after `StereoFieldShaper::processBlock`, and add the rotary Output control with an APVTS attachment.**
- [x] **Step 4: Bump the project version to 2.2.0 and document the Output control.**
- [x] **Step 5: Run the host checks and full CTest suite.** Expected: host checks pass; CTest has zero failures.
- [x] **Step 6: Commit the feature and plan** with the personal noreply Git identity.
