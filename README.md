# System Audio Recorder

Qt 6 desktop application for recording speaker or microphone output to a 16-bit PCM WAV file.

## Backends

- Windows: WASAPI loopback via miniaudio.
- Linux: PulseAudio backend via miniaudio. This also works with PipeWire when the PulseAudio compatibility server is enabled.

## Prerequisites

- CMake 3.21+
- C++20 compiler
- Qt 6.5+ with `Core`, `Gui`, and `Widgets`
- Internet access during the first configure step so CMake can fetch `miniaudio`

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Notes

- The device picker shows playback and recording devices
- Record writes captured samples to memory immediately
- Stop ends capture and prompts for a `.wav` output path