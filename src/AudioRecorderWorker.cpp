#include "AudioRecorderWorker.h"
#include "AudioRecorderWorkerConfig.h"
#include "AudioRecorderWorkerPlatform.h"
#include "AudioRecorderWorkerState.h"
#include "WavWriterStream.h"
#include <QTimer>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>

namespace {

using AudioRecorderWorkerDetail::ActiveCaptureSource;
using AudioRecorderWorkerDetail::DeviceEntry;
using AudioRecorderWorkerDetail::EnumeratedDevices;

QString backendErrorMessage(const char* action, ma_result result)
{
    return QStringLiteral("%1: %2")
        .arg(QString::fromUtf8(action), QString::fromUtf8(ma_result_description(result)));
}

qint16 peakSampleForBuffer(const qint16* inputSamples, std::size_t sampleCount)
{
    qint16 peakSample = 0;

    for (std::size_t index = 0; index < sampleCount; ++index) {
        const auto magnitude = static_cast<qint16>(std::clamp(
            std::abs(static_cast<int>(inputSamples[index])),
            0,
            AudioRecorderWorkerConfig::MaxSampleMagnitude));
        peakSample = std::max(peakSample, magnitude);
    }

    return peakSample;
}

float decibelForPeak(float peak)
{
    return peak > AudioRecorderWorkerConfig::SilenceThreshold
        ? std::clamp(20.0f * std::log10(peak), AudioRecorderWorkerConfig::MinDb, 0.0f)
        : AudioRecorderWorkerConfig::MinDb;
}

void captureCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount)
{
    Q_UNUSED(output);

    auto* source = static_cast<ActiveCaptureSource*>(device->pUserData);
    if (source == nullptr || source->owner == nullptr || input == nullptr) {
        return;
    }

    const ma_uint32 channelCount = device->capture.channels;
    const auto sampleCount = static_cast<std::size_t>(frameCount) * channelCount;
    const auto* inputSamples = static_cast<const qint16*>(input);
    const float peakLinear = static_cast<float>(peakSampleForBuffer(inputSamples, sampleCount)) /
        AudioRecorderWorkerConfig::PeakScale;
    const float adjustedPeak = source->muted
        ? 0.0f
        : std::clamp(peakLinear * source->gainLinear, 0.0f, 1.0f);

    if (source->owner->recording) {
        std::scoped_lock lock(source->bufferMutex);
        source->pendingSamples.insert(source->pendingSamples.end(), inputSamples, inputSamples + sampleCount);
    }

    float currentPeak = source->peakLinear.load(std::memory_order_relaxed);
    while (adjustedPeak > currentPeak &&
           !source->peakLinear.compare_exchange_weak(
               currentPeak,
               adjustedPeak,
               std::memory_order_release,
               std::memory_order_relaxed)) {
    }
}

}

AudioRecorderWorker::AudioRecorderWorker(QObject* parent)
    : QObject(parent)
    , m_levelTimer(new QTimer(this))
    , m_state(new AudioRecorderWorkerState)
    , m_writer(std::make_unique<WavWriterDetail::WavWriterStream>())
{
    m_levelTimer->setInterval(50);
    connect(m_levelTimer, &QTimer::timeout, this, &AudioRecorderWorker::publishLevel);
}

AudioRecorderWorker::~AudioRecorderWorker()
{
    shutdownDevices();
    uninitializeContext();
    delete m_state;
}

bool AudioRecorderWorker::ensureContext()
{
    if (m_state->contextReady) {
        return true;
    }

    if (!AudioRecorderWorkerPlatform::supportsAudioCapture()) {
        emit errorOccurred(AudioRecorderWorkerPlatform::unsupportedPlatformMessage());
        return false;
    }

    std::array<ma_backend, AudioRecorderWorkerPlatform::SupportedBackendCapacity> backends{};
    const ma_uint32 backendCount = AudioRecorderWorkerPlatform::supportedBackends(backends);
    const ma_result result = ma_context_init(
        backendCount > 0 ? backends.data() : nullptr,
        backendCount,
        nullptr,
        &m_state->context);
    if (result != MA_SUCCESS) {
        emit errorOccurred(backendErrorMessage("Failed to initialize audio backend", result));
        return false;
    }

    m_state->contextReady = true;
    return true;
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
    if (!m_state->contextReady) {
        return;
    }

    ma_context_uninit(&m_state->context);
    m_state->contextReady = false;
    m_state->devices.clear();
    m_state->activeSources.clear();
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
        &m_state->context,
        &playbackDevices,
        &playbackCount,
        &captureDevices,
        &captureCount);
    if (result != MA_SUCCESS) {
        emit errorOccurred(backendErrorMessage("Failed to enumerate playback devices", result));
        return;
    }

    const EnumeratedDevices devices = AudioRecorderWorkerPlatform::enumerateDevices(
        playbackDevices,
        playbackCount,
        captureDevices,
        captureCount);

    m_state->devices = devices.devices;
    if (!devices.emptyMessage.isEmpty()) {
        emit errorOccurred(devices.emptyMessage);
    }

    emit devicesReady(devices.names, devices.defaultIndex);
}

bool AudioRecorderWorker::flushPendingSamples(QString* errorMessage)
{
    std::size_t mixSampleCount = 0;
    std::size_t minAvailableSamples = std::numeric_limits<std::size_t>::max();
    int unmutedSourceCount = 0;

    for (const auto& activeSource : m_state->activeSources) {
        std::scoped_lock lock(activeSource->bufferMutex);

        if (activeSource->muted) {
            activeSource->pendingSamples.clear();
            continue;
        }

        ++unmutedSourceCount;
        minAvailableSamples = std::min(minAvailableSamples, activeSource->pendingSamples.size());
        mixSampleCount = std::max(mixSampleCount, activeSource->pendingSamples.size());
    }

    if (unmutedSourceCount == 0) {
        return true;
    }

    mixSampleCount = unmutedSourceCount == 1 ? mixSampleCount : minAvailableSamples;
    if (mixSampleCount == 0 || mixSampleCount == std::numeric_limits<std::size_t>::max()) {
        return true;
    }

    std::vector<int> mixedSamples(mixSampleCount, 0);

    for (const auto& activeSource : m_state->activeSources) {
        if (activeSource->muted) {
            continue;
        }

        std::scoped_lock lock(activeSource->bufferMutex);
        for (std::size_t sampleIndex = 0; sampleIndex < mixSampleCount; ++sampleIndex) {
            mixedSamples[sampleIndex] += static_cast<int>(
                activeSource->pendingSamples[sampleIndex] * activeSource->gainLinear);
        }

        for (std::size_t sampleIndex = 0; sampleIndex < mixSampleCount; ++sampleIndex) {
            activeSource->pendingSamples.pop_front();
        }
    }

    std::vector<qint16> clampedSamples;
    clampedSamples.reserve(mixSampleCount);
    for (int sample : mixedSamples) {
        clampedSamples.push_back(static_cast<qint16>(std::clamp(
            sample,
            -AudioRecorderWorkerConfig::MaxSampleMagnitude - 1,
            AudioRecorderWorkerConfig::MaxSampleMagnitude)));
    }

    return m_writer->appendSamples(
        std::span<const qint16>(clampedSamples.data(), clampedSamples.size()),
        errorMessage);
}

bool AudioRecorderWorker::startCaptureSources(
    const QVector<int>& deviceIndices,
    const QVector<bool>& mutedStates,
    const QVector<int>& gainPercents)
{
    if (!ensureContext()) {
        return false;
    }

    shutdownDevices();
    m_state->activeSources.clear();

    if (deviceIndices.isEmpty()) {
        emit errorOccurred(QStringLiteral("Select at least one audio source."));
        return false;
    }

    for (int sourceIndex = 0; sourceIndex < deviceIndices.size(); ++sourceIndex) {
        const int deviceIndex = deviceIndices[sourceIndex];
        if (deviceIndex < 0 || deviceIndex >= m_state->devices.size()) {
            shutdownDevices();
            emit errorOccurred(QStringLiteral("Select a valid audio device."));
            return false;
        }

        auto activeSource = std::make_unique<ActiveCaptureSource>();
        activeSource->owner = m_state;
        activeSource->device = m_state->devices[deviceIndex];
        activeSource->muted = mutedStates.value(sourceIndex, false);
        activeSource->gainLinear = std::max(0, gainPercents.value(sourceIndex, 100)) / 100.0f;

        ma_device_config deviceConfig = AudioRecorderWorkerPlatform::createDeviceConfig(activeSource->device);
        deviceConfig.dataCallback = captureCallback;
        deviceConfig.pUserData = activeSource.get();

        const ma_result initResult = ma_device_init(
            &m_state->context,
            &deviceConfig,
            &activeSource->captureDevice);
        if (initResult != MA_SUCCESS) {
            shutdownDevices();
            emit errorOccurred(backendErrorMessage("Failed to initialize audio capture", initResult));
            return false;
        }

        activeSource->deviceReady = true;
        if (m_state->activeSources.empty()) {
            m_state->sampleRate = activeSource->captureDevice.sampleRate;
            m_state->channelCount = static_cast<quint16>(activeSource->captureDevice.capture.channels);
        }

        m_state->activeSources.push_back(std::move(activeSource));
    }

    for (const auto& activeSource : m_state->activeSources) {
        const ma_result startResult = ma_device_start(&activeSource->captureDevice);
        if (startResult != MA_SUCCESS) {
            shutdownDevices();
            emit errorOccurred(backendErrorMessage("Failed to start audio capture", startResult));
            return false;
        }
    }

    return true;
}

bool AudioRecorderWorker::updateActiveSourceMix(
    const QVector<int>& deviceIndices,
    const QVector<bool>& mutedStates,
    const QVector<int>& gainPercents)
{
    if (deviceIndices.size() != static_cast<qsizetype>(m_state->activeSources.size())) {
        return false;
    }

    for (int index = 0; index < deviceIndices.size(); ++index) {
        const auto& activeSource = m_state->activeSources[index];
        const int deviceIndex = deviceIndices[index];
        if (deviceIndex < 0 || deviceIndex >= m_state->devices.size()) {
            return false;
        }

        if (activeSource->device.name != m_state->devices[deviceIndex].name ||
            activeSource->device.kind != m_state->devices[deviceIndex].kind) {
            return false;
        }

        activeSource->muted = mutedStates.value(index, false);
        activeSource->gainLinear = std::max(0, gainPercents.value(index, 100)) / 100.0f;
    }

    return true;
}

void AudioRecorderWorker::configureSources(
    const QVector<int>& deviceIndices,
    const QVector<bool>& mutedStates,
    const QVector<int>& gainPercents)
{
    if (deviceIndices.isEmpty()) {
        shutdownDevices();
        emit sourceLevelsChanged({});
        emit levelChanged(AudioRecorderWorkerConfig::MinDb);
        return;
    }

    if (!updateActiveSourceMix(deviceIndices, mutedStates, gainPercents) &&
        !startCaptureSources(deviceIndices, mutedStates, gainPercents)) {
        return;
    }
}

void AudioRecorderWorker::startRecording(const QString& filePath)
{
    if (filePath.isEmpty()) {
        emit errorOccurred(QStringLiteral("No output path was selected."));
        return;
    }

    if (m_state->activeSources.empty()) {
        emit errorOccurred(QStringLiteral("Select at least one audio source."));
        return;
    }

    for (const auto& activeSource : m_state->activeSources) {
        std::scoped_lock lock(activeSource->bufferMutex);
        activeSource->pendingSamples.clear();
        activeSource->peakLinear.store(0.0f, std::memory_order_relaxed);
    }

    QString errorMessage;
    if (!m_writer->open(filePath, m_state->sampleRate, m_state->channelCount, &errorMessage)) {
        emit errorOccurred(QStringLiteral("Failed to open WAV file: %1").arg(errorMessage));
        return;
    }

    m_state->recording = true;
    emit recordingStateChanged(true);
}

void AudioRecorderWorker::stopRecording()
{
    m_state->recording = false;
    emit recordingStateChanged(false);

    QString errorMessage;
    if (!flushPendingSamples(&errorMessage)) {
        m_writer->discard();
        emit errorOccurred(QStringLiteral("Failed to write WAV file: %1").arg(errorMessage));
        return;
    }

    if (!m_writer->isOpen() || !m_writer->hasAudio()) {
        m_writer->discard();
        emit recordingStopped(false);
        return;
    }

    const QString filePath = m_writer->filePath();
    if (!m_writer->finalize(&errorMessage)) {
        m_writer->discard();
        emit errorOccurred(QStringLiteral("Failed to finalize WAV file: %1").arg(errorMessage));
        return;
    }

    emit recordingSaved(filePath);
}

void AudioRecorderWorker::discardRecording()
{
    for (const auto& activeSource : m_state->activeSources) {
        {
            std::scoped_lock lock(activeSource->bufferMutex);
            activeSource->pendingSamples.clear();
        }
        activeSource->peakLinear.store(0.0f, std::memory_order_relaxed);
    }

    m_state->recording = false;
    emit recordingStateChanged(false);
    m_writer->discard();
}

void AudioRecorderWorker::publishLevel()
{
    if (m_state->recording) {
        QString errorMessage;
        if (!flushPendingSamples(&errorMessage)) {
            shutdownDevices();
            m_writer->discard();
            emit errorOccurred(QStringLiteral("Failed to write WAV file: %1").arg(errorMessage));
        }
    }

    QVector<float> sourceLevelsDb;
    sourceLevelsDb.reserve(m_state->activeSources.size());

    float overallPeak = 0.0f;
    for (const auto& activeSource : m_state->activeSources) {
        const float sourcePeak = activeSource->peakLinear.exchange(0.0f, std::memory_order_acq_rel);
        sourceLevelsDb.push_back(decibelForPeak(sourcePeak));
        overallPeak = std::max(overallPeak, sourcePeak);
    }

    emit sourceLevelsChanged(sourceLevelsDb);
    emit levelChanged(decibelForPeak(overallPeak));
}

void AudioRecorderWorker::shutdownDevices()
{
    if (m_state->activeSources.empty()) {
        return;
    }

    for (const auto& activeSource : m_state->activeSources) {
        if (activeSource->deviceReady) {
            ma_device_uninit(&activeSource->captureDevice);
        }
    }

    m_state->activeSources.clear();
}
