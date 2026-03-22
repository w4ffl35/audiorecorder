#include "AudioRecorderWorkerPlatform.h"
#include "AudioRecorderWorkerConfig.h"
#include <limits>

namespace {

using AudioRecorderWorkerDetail::CaptureEntry;
using AudioRecorderWorkerDetail::DeviceEntry;
using AudioRecorderWorkerDetail::EnumeratedDevices;

QString normalizedName(QString text)
{
    text = text.toLower();
    text.remove(QStringLiteral("monitor of "));
    text.remove(QStringLiteral(".monitor"));
    text.remove(QStringLiteral(" monitor"));
    text.remove(QStringLiteral("default"));

    QString normalized;
    normalized.reserve(text.size());
    for (const QChar ch : text) {
        if (ch.isLetterOrNumber()) {
            normalized.append(ch);
        }
    }

    return normalized;
}

bool isMonitorDeviceName(const QString& name)
{
    return name.contains(QStringLiteral("monitor"), Qt::CaseInsensitive);
}

int computeMonitorMatchScore(const QString& playbackName, const CaptureEntry& capture)
{
    if (!isMonitorDeviceName(capture.name)) {
        return std::numeric_limits<int>::min();
    }

    const QString playbackNorm = normalizedName(playbackName);
    const QString captureNorm = normalizedName(capture.name);
    int score = 0;

    if (!playbackNorm.isEmpty() && captureNorm == playbackNorm) {
        score += 200;
    }

    if (!playbackNorm.isEmpty() && captureNorm.contains(playbackNorm)) {
        score += 120;
    }

    if (!playbackName.isEmpty() &&
        capture.name.contains(playbackName, Qt::CaseInsensitive)) {
        score += 80;
    }

    if (capture.isDefault) {
        score += 10;
    }

    return score;
}

QVector<CaptureEntry> collectCaptureEntries(
    ma_device_info* captureDevices,
    ma_uint32 captureCount)
{
    QVector<CaptureEntry> captureEntries;
    captureEntries.reserve(static_cast<qsizetype>(captureCount));

    for (ma_uint32 index = 0; index < captureCount; ++index) {
        CaptureEntry entry;
        entry.name = QString::fromUtf8(captureDevices[index].name);
        entry.id = captureDevices[index].id;
        entry.isDefault = captureDevices[index].isDefault == MA_TRUE;
        captureEntries.push_back(entry);
    }

    return captureEntries;
}

bool assignMonitorCaptureDevice(
    DeviceEntry& playbackEntry,
    const QVector<CaptureEntry>& captureEntries)
{
    int bestScore = std::numeric_limits<int>::min();

    for (const CaptureEntry& captureEntry : captureEntries) {
        const int score = computeMonitorMatchScore(playbackEntry.name, captureEntry);
        if (score > bestScore) {
            bestScore = score;
            playbackEntry.captureId = captureEntry.id;
        }
    }

    return bestScore >= 0;
}

}

namespace AudioRecorderWorkerPlatform {

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
    backends[0] = ma_backend_pulseaudio;
    return 1;
}

EnumeratedDevices enumerateDevices(
    ma_device_info* playbackDevices,
    ma_uint32 playbackCount,
    ma_device_info* captureDevices,
    ma_uint32 captureCount)
{
    EnumeratedDevices enumerated;
    const QVector<CaptureEntry> captureEntries = collectCaptureEntries(
        captureDevices,
        captureCount);

    for (ma_uint32 index = 0; index < playbackCount; ++index) {
        DeviceEntry entry;
        entry.name = QString::fromUtf8(playbackDevices[index].name);
        entry.playbackId = playbackDevices[index].id;
        entry.isDefault = playbackDevices[index].isDefault == MA_TRUE;

        if (!assignMonitorCaptureDevice(entry, captureEntries)) {
            continue;
        }

        if (entry.isDefault && enumerated.defaultIndex < 0) {
            enumerated.defaultIndex = enumerated.names.size();
        }

        enumerated.devices.push_back(entry);
        enumerated.names.push_back(
            entry.isDefault
                ? QStringLiteral("%1 (Default)").arg(entry.name)
                : entry.name);
    }

    if (enumerated.names.isEmpty()) {
        enumerated.emptyMessage = QStringLiteral(
            "No PulseAudio/PipeWire monitor streams were found for the available playback devices.");
    }

    return enumerated;
}

ma_device_config createDeviceConfig(const DeviceEntry& deviceEntry)
{
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.pDeviceID = &deviceEntry.captureId;
    deviceConfig.capture.format = AudioRecorderWorkerConfig::CaptureFormat;
    deviceConfig.capture.channels = AudioRecorderWorkerConfig::CaptureChannels;
    deviceConfig.capture.shareMode = ma_share_mode_shared;
    deviceConfig.sampleRate = AudioRecorderWorkerConfig::CaptureSampleRate;
    return deviceConfig;
}

}
