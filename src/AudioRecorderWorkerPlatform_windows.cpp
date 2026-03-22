#include "AudioRecorderWorkerPlatform.h"
#include "AudioRecorderWorkerConfig.h"

namespace AudioRecorderWorkerPlatform {

using AudioRecorderWorkerDetail::DeviceEntry;
using AudioRecorderWorkerDetail::DeviceKind;
using AudioRecorderWorkerDetail::EnumeratedDevices;

namespace {

QString displayName(const QString& name, DeviceKind kind, bool isDefault)
{
    const QString kindSuffix = kind == DeviceKind::Output
        ? QStringLiteral("Output")
        : QStringLiteral("Input");
    const QString baseName = QStringLiteral("%1 (%2)").arg(name, kindSuffix);
    return isDefault ? QStringLiteral("%1 (Default)").arg(baseName) : baseName;
}

}

bool supportsAudioCapture()
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
        entry.kind = DeviceKind::Output;
        entry.isDefault = playbackDevices[index].isDefault == MA_TRUE;

        if (entry.isDefault && enumerated.defaultIndex < 0) {
            enumerated.defaultIndex = enumerated.names.size();
        }

        enumerated.devices.push_back(entry);
        enumerated.names.push_back(displayName(entry.name, entry.kind, entry.isDefault));
    }

    for (ma_uint32 index = 0; index < captureCount; ++index) {
        DeviceEntry entry;
        entry.name = QString::fromUtf8(captureDevices[index].name);
        entry.captureId = captureDevices[index].id;
        entry.kind = DeviceKind::Input;
        entry.isDefault = captureDevices[index].isDefault == MA_TRUE;

        if (entry.isDefault && enumerated.defaultIndex < 0) {
            enumerated.defaultIndex = enumerated.names.size();
        }

        enumerated.devices.push_back(entry);
        enumerated.names.push_back(displayName(entry.name, entry.kind, entry.isDefault));
    }

    return enumerated;
}

ma_device_config createDeviceConfig(const DeviceEntry& deviceEntry)
{
    const ma_device_type deviceType = deviceEntry.kind == DeviceKind::Output
        ? ma_device_type_loopback
        : ma_device_type_capture;
    ma_device_config deviceConfig = ma_device_config_init(deviceType);
    deviceConfig.capture.pDeviceID = deviceEntry.kind == DeviceKind::Output
        ? &deviceEntry.playbackId
        : &deviceEntry.captureId;
    deviceConfig.capture.format = AudioRecorderWorkerConfig::CaptureFormat;
    deviceConfig.capture.channels = AudioRecorderWorkerConfig::CaptureChannels;
    deviceConfig.capture.shareMode = ma_share_mode_shared;
    deviceConfig.sampleRate = AudioRecorderWorkerConfig::CaptureSampleRate;
    return deviceConfig;
}

}
