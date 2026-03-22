#pragma once

#include <miniaudio.h>

namespace AudioRecorderWorkerConfig {

inline constexpr ma_format CaptureFormat = ma_format_s16;
inline constexpr ma_uint32 CaptureChannels = 2;
inline constexpr ma_uint32 CaptureSampleRate = 48000;
inline constexpr float MinDb = -60.0f;
inline constexpr float SilenceThreshold = 0.00001f;
inline constexpr float PeakScale = 32768.0f;
inline constexpr int MaxSampleMagnitude = 32767;

}
