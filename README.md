# System Audio Recorder

[![Release](https://img.shields.io/github/v/release/w4ffl35/audiorecorder?display_name=tag)](https://github.com/w4ffl35/audiorecorder/releases/latest)
[![Release Build](https://github.com/w4ffl35/audiorecorder/actions/workflows/release.yml/badge.svg)](https://github.com/w4ffl35/audiorecorder/actions/workflows/release.yml)
[![License](https://img.shields.io/github/license/w4ffl35/audiorecorder)](https://github.com/w4ffl35/audiorecorder/blob/master/LICENSE.md)

![alt text](image.png)

Download prebuilt Linux and Windows binaries from [GitHub Releases](https://github.com/w4ffl35/audiorecorder/releases/latest).

Qt 6 desktop application for recording and mixing multiple audio input and output sources into a 16-bit PCM WAV file.

This code was generated with AI. I wanted a very simple audio recorder for a project and did not like the available options. Its ugly, but it works.

Current release: `v1.2.0`

## Backends

- Windows: WASAPI loopback via miniaudio.
- Linux: PulseAudio monitor capture via miniaudio. This also works with PipeWire when the PulseAudio compatibility server is enabled.

## Prerequisites

- CMake 3.21+
- C++20 compiler
- Qt 6.4+ with `Core`, `Gui`, and `Widgets`
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

- Pushing a tag like `v1.2.0` triggers GitHub Actions to build release artifacts for Linux and Windows.
- GitHub Actions publishes the built archives to the matching GitHub release.

## Notes

- You can configure one or more audio sources at the same time, including output devices and microphones.
- Each source has its own device selector, mute toggle, level meter, and gain slider.
- A source can only be selected once across the active source list.
- The bottom level meter shows the overall mixed signal.
- Press Record to choose the destination `.wav` file before capture starts.
- While recording, audio from all unmuted sources is mixed and streamed directly to the selected file.
- Stop ends capture, finalizes the WAV header, and closes the file.
- If recording ends without captured audio, the incomplete output file is discarded.
