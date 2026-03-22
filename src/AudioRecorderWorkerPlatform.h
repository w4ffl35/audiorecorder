#pragma once

#include "AudioRecorderWorkerState.h"
#include <array>

namespace AudioRecorderWorkerPlatform {

inline constexpr std::size_t SupportedBackendCapacity = 1;

bool supportsSpeakerCapture();
QString unsupportedPlatformMessage();
ma_uint32 supportedBackends(
    std::array<ma_backend, SupportedBackendCapacity>& backends);
AudioRecorderWorkerDetail::EnumeratedDevices enumerateDevices(
    ma_device_info* playbackDevices,
    ma_uint32 playbackCount,
    ma_device_info* captureDevices,
    ma_uint32 captureCount);
ma_device_config createDeviceConfig(const AudioRecorderWorkerDetail::DeviceEntry& deviceEntry);

}
