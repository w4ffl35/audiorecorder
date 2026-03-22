#include "AudioRecorderWorkerPlatform.h"
#include "AudioRecorderWorkerConfig.h"

namespace AudioRecorderWorkerPlatform {

using AudioRecorderWorkerDetail::DeviceEntry;
using AudioRecorderWorkerDetail::EnumeratedDevices;

bool supportsSpeakerCapture()
{
    return false;
}

QString unsupportedPlatformMessage()
{
    return QStringLiteral("This example currently supports Windows and Linux only.");
}

ma_uint32 supportedBackends(
    std::array<ma_backend, SupportedBackendCapacity>& backends)
{
    Q_UNUSED(backends);
    return 0;
}

EnumeratedDevices enumerateDevices(
    ma_device_info* playbackDevices,
    ma_uint32 playbackCount,
    ma_device_info* captureDevices,
    ma_uint32 captureCount)
{
    Q_UNUSED(playbackDevices);
    Q_UNUSED(playbackCount);
    Q_UNUSED(captureDevices);
    Q_UNUSED(captureCount);
    return {};
}

ma_device_config createDeviceConfig(const DeviceEntry& deviceEntry)
{
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.pDeviceID = &deviceEntry.playbackId;
    deviceConfig.capture.format = AudioRecorderWorkerConfig::CaptureFormat;
    deviceConfig.capture.channels = AudioRecorderWorkerConfig::CaptureChannels;
    deviceConfig.capture.shareMode = ma_share_mode_shared;
    deviceConfig.sampleRate = AudioRecorderWorkerConfig::CaptureSampleRate;
    return deviceConfig;
}

}
