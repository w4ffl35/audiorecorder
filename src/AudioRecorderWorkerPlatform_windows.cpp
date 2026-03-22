#include "AudioRecorderWorkerPlatform.h"
#include "AudioRecorderWorkerConfig.h"

namespace AudioRecorderWorkerPlatform {

using AudioRecorderWorkerDetail::DeviceEntry;
using AudioRecorderWorkerDetail::EnumeratedDevices;

bool supportsSpeakerCapture()
{
    return true;
}

QString unsupportedPlatformMessage()
{
    return QStringLiteral("This example currently supports Windows and Linux only.");
}

ma_uint32 supportedBackends(
    std::array<ma_backend, SupportedBackendCapacity>& backends)
{
    backends[0] = ma_backend_wasapi;
    return 1;
}

EnumeratedDevices enumerateDevices(
    ma_device_info* playbackDevices,
    ma_uint32 playbackCount,
    ma_device_info* captureDevices,
    ma_uint32 captureCount)
{
    Q_UNUSED(captureDevices);
    Q_UNUSED(captureCount);

    EnumeratedDevices enumerated;
    for (ma_uint32 index = 0; index < playbackCount; ++index) {
        DeviceEntry entry;
        entry.name = QString::fromUtf8(playbackDevices[index].name);
        entry.playbackId = playbackDevices[index].id;
        entry.captureId = playbackDevices[index].id;
        entry.isDefault = playbackDevices[index].isDefault == MA_TRUE;

        if (entry.isDefault && enumerated.defaultIndex < 0) {
            enumerated.defaultIndex = enumerated.names.size();
        }

        enumerated.devices.push_back(entry);
        enumerated.names.push_back(
            entry.isDefault
                ? QStringLiteral("%1 (Default)").arg(entry.name)
                : entry.name);
    }

    return enumerated;
}

ma_device_config createDeviceConfig(const DeviceEntry& deviceEntry)
{
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_loopback);
    deviceConfig.capture.pDeviceID = &deviceEntry.playbackId;
    deviceConfig.capture.format = AudioRecorderWorkerConfig::CaptureFormat;
    deviceConfig.capture.channels = AudioRecorderWorkerConfig::CaptureChannels;
    deviceConfig.capture.shareMode = ma_share_mode_shared;
    deviceConfig.sampleRate = AudioRecorderWorkerConfig::CaptureSampleRate;
    return deviceConfig;
}

}
