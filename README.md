# System Audio Recorder

Qt 6 desktop application for recording speaker or microphone output to a 16-bit PCM WAV file.

This code was generated with AI. I wanted a very simple audio recorder for a project and did not like the available options. Its ugly, but it works.

Current release: `v1.0.0`

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
bash build.sh
```

Or

```bash
cmake -S . -B build
cmake --build build
```

## Releases

- Pushing a tag like `v1.0.0` triggers GitHub Actions to build release artifacts for Linux and Windows.
- GitHub Actions publishes the built archives to the matching GitHub release.

## Notes

- The device picker shows playback devices only.
- Record writes captured samples to memory immediately.
- Stop ends capture and prompts for a `.wav` output path.
