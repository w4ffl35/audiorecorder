#include "AudioRecorderWorker.h"

#include "WavWriter.h"

#include <QStringList>
#include <QTimer>
#include <QVector>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

#include <miniaudio.h>

namespace {

#if defined(_WIN32)
constexpr ma_backend kBackends[] = {ma_backend_wasapi};
#elif defined(__linux__)
constexpr ma_backend kBackends[] = {ma_backend_pulseaudio};
#else
constexpr ma_backend kBackends[] = {};
#endif

constexpr ma_format kCaptureFormat = ma_format_s16;
constexpr ma_uint32 kCaptureChannels = 2;
constexpr ma_uint32 kCaptureSampleRate = 48000;
constexpr float kMinDb = -60.0f;

QString backendErrorMessage(const char* action, ma_result result)
{
    return QStringLiteral("%1: %2").arg(QString::fromUtf8(action), QString::fromUtf8(ma_result_description(result)));
}

struct DeviceEntry
{
    QString name;
    ma_device_id id;
    bool isDefault = false;
};

} 

class AudioRecorderWorkerPrivate
{
public:
    ma_context context{};
    ma_device device{};
    QVector<DeviceEntry> devices;
    std::vector<qint16> capturedSamples;
    std::mutex bufferMutex;
    std::atomic<float> peakLinear = 0.0f;
    bool contextReady = false;
    bool deviceReady = false;
    bool recording = false;
    quint32 sampleRate = kCaptureSampleRate;
    quint16 channelCount = kCaptureChannels;
};

AudioRecorderWorker::AudioRecorderWorker(QObject* parent)
    : QObject(parent)
    , m_levelTimer(new QTimer(this))
    , m_privateState(new AudioRecorderWorkerPrivate)
{
    m_levelTimer->setInterval(50);
    connect(m_levelTimer, &QTimer::timeout, this, &AudioRecorderWorker::publishLevel);
}

AudioRecorderWorker::~AudioRecorderWorker()
{
    shutdownDevice();
    uninitializeContext();
    delete m_privateState;
}

bool AudioRecorderWorker::ensureContext()
{
    if (m_privateState->contextReady) {
        return true;
    }

#if !defined(_WIN32) && !defined(__linux__)
    emit errorOccurred(QStringLiteral("This example currently supports Windows and Linux only."));
    return false;
#else
    const ma_result result = ma_context_init(kBackends, std::size(kBackends), nullptr, &m_privateState->context);
    if (result != MA_SUCCESS) {
        emit errorOccurred(backendErrorMessage("Failed to initialize audio backend", result));
        return false;
    }

    m_privateState->contextReady = true;
    return true;
#endif
}

void AudioRecorderWorker::initialize()
{
    if (!m_levelTimer->isActive()) {
        m_levelTimer->start();
    }

    refreshDevices();
}

void AudioRecorderWorker::uninitializeContext()
{
    if (!m_privateState->contextReady) {
        return;
    }

    ma_context_uninit(&m_privateState->context);
    m_privateState->contextReady = false;
    m_privateState->devices.clear();
}

void AudioRecorderWorker::refreshDevices()
{
    if (!ensureContext()) {
        return;
    }

    ma_device_info* playbackDevices = nullptr;
    ma_uint32 playbackCount = 0;
    ma_device_info* captureDevices = nullptr;
    ma_uint32 captureCount = 0;

    const ma_result result = ma_context_get_devices(
        &m_privateState->context,
        &playbackDevices,
        &playbackCount,
        &captureDevices,
        &captureCount);
    Q_UNUSED(captureDevices);
    Q_UNUSED(captureCount);

    if (result != MA_SUCCESS) {
        emit errorOccurred(backendErrorMessage("Failed to enumerate playback devices", result));
        return;
    }

    m_privateState->devices.clear();
    QStringList names;
    int defaultIndex = -1;

    for (ma_uint32 i = 0; i < playbackCount; ++i) {
        DeviceEntry entry;
        entry.name = QString::fromUtf8(playbackDevices[i].name);
        entry.id = playbackDevices[i].id;
        entry.isDefault = playbackDevices[i].isDefault == MA_TRUE;

        if (entry.isDefault && defaultIndex < 0) {
            defaultIndex = names.size();
        }

        m_privateState->devices.push_back(entry);
        names.push_back(entry.isDefault ? QStringLiteral("%1 (Default)").arg(entry.name) : entry.name);
    }

    emit devicesReady(names, defaultIndex);
}

void AudioRecorderWorker::startRecording(int deviceIndex)
{
    if (!ensureContext()) {
        return;
    }

    if (deviceIndex < 0 || deviceIndex >= m_privateState->devices.size()) {
        emit errorOccurred(QStringLiteral("Select a valid playback device before recording."));
        return;
    }

    shutdownDevice();

    {
        std::scoped_lock lock(m_privateState->bufferMutex);
        m_privateState->capturedSamples.clear();
    }
    m_privateState->peakLinear.store(0.0f, std::memory_order_relaxed);

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_loopback);
    deviceConfig.capture.pDeviceID = &m_privateState->devices[deviceIndex].id;
    deviceConfig.capture.format = kCaptureFormat;
    deviceConfig.capture.channels = kCaptureChannels;
    deviceConfig.capture.shareMode = ma_share_mode_shared;
    deviceConfig.sampleRate = kCaptureSampleRate;
    deviceConfig.dataCallback = [](ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
        Q_UNUSED(output);

        auto* worker = static_cast<AudioRecorderWorker*>(device->pUserData);
        if (worker == nullptr || input == nullptr || !worker->m_privateState->recording) {
            return;
        }

        const ma_uint32 channelCount = device->capture.channels;
        const auto sampleCount = static_cast<size_t>(frameCount) * channelCount;
        const auto* inputSamples = static_cast<const qint16*>(input);

        qint16 peakSample = 0;
        for (size_t i = 0; i < sampleCount; ++i) {
            const auto magnitude = static_cast<qint16>(std::clamp(std::abs(static_cast<int>(inputSamples[i])), 0, 32767));
            peakSample = std::max(peakSample, magnitude);
        }

        {
            std::scoped_lock lock(worker->m_privateState->bufferMutex);
            worker->m_privateState->capturedSamples.insert(
                worker->m_privateState->capturedSamples.end(),
                inputSamples,
                inputSamples + sampleCount);
        }

        worker->publishPeak(static_cast<float>(peakSample) / 32768.0f);
    };
    deviceConfig.pUserData = this;

    const ma_result initResult = ma_device_init(&m_privateState->context, &deviceConfig, &m_privateState->device);
    if (initResult != MA_SUCCESS) {
        emit errorOccurred(backendErrorMessage("Failed to initialize loopback capture", initResult));
        return;
    }

    m_privateState->deviceReady = true;
    m_privateState->sampleRate = m_privateState->device.sampleRate;
    m_privateState->channelCount = static_cast<quint16>(m_privateState->device.capture.channels);
    m_privateState->recording = true;

    const ma_result startResult = ma_device_start(&m_privateState->device);
    if (startResult != MA_SUCCESS) {
        shutdownDevice();
        emit errorOccurred(backendErrorMessage("Failed to start loopback capture", startResult));
        return;
    }

    emit recordingStateChanged(true);
}

void AudioRecorderWorker::stopRecording()
{
    const bool hadAudio = [&]() {
        std::scoped_lock lock(m_privateState->bufferMutex);
        return !m_privateState->capturedSamples.empty();
    }();

    shutdownDevice();
    emit levelChanged(kMinDb);
    emit recordingStopped(hadAudio);
}

void AudioRecorderWorker::saveRecording(const QString& filePath)
{
    if (filePath.isEmpty()) {
        emit errorOccurred(QStringLiteral("No output path was selected."));
        return;
    }

    std::vector<qint16> samples;
    {
        std::scoped_lock lock(m_privateState->bufferMutex);
        samples = m_privateState->capturedSamples;
    }

    if (samples.empty()) {
        emit errorOccurred(QStringLiteral("There is no captured audio to save."));
        return;
    }

    QString errorMessage;
    if (!WavWriter::writePcm16(
            filePath,
            std::span<const qint16>(samples.data(), samples.size()),
            m_privateState->sampleRate,
            m_privateState->channelCount,
            &errorMessage)) {
        emit errorOccurred(QStringLiteral("Failed to save WAV file: %1").arg(errorMessage));
        return;
    }

    emit recordingSaved(filePath);
}

void AudioRecorderWorker::discardRecording()
{
    std::scoped_lock lock(m_privateState->bufferMutex);
    m_privateState->capturedSamples.clear();
}

void AudioRecorderWorker::publishLevel()
{
    const float peak = m_privateState->peakLinear.exchange(0.0f, std::memory_order_acq_rel);
    const float db = peak > 0.00001f ? std::clamp(20.0f * std::log10(peak), kMinDb, 0.0f) : kMinDb;
    emit levelChanged(db);
}

void AudioRecorderWorker::publishPeak(float linearPeak)
{
    float currentPeak = m_privateState->peakLinear.load(std::memory_order_relaxed);
    while (linearPeak > currentPeak &&
           !m_privateState->peakLinear.compare_exchange_weak(
               currentPeak,
               linearPeak,
               std::memory_order_release,
               std::memory_order_relaxed)) {
    }
}

void AudioRecorderWorker::shutdownDevice()
{
    if (!m_privateState->deviceReady) {
        m_privateState->recording = false;
        emit recordingStateChanged(false);
        return;
    }

    m_privateState->recording = false;
    ma_device_uninit(&m_privateState->device);
    m_privateState->deviceReady = false;
    emit recordingStateChanged(false);
}
