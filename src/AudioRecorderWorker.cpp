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
#include <span>

namespace {

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

void publishPeak(AudioRecorderWorkerState* state, float linearPeak)
{
    float currentPeak = state->peakLinear.load(std::memory_order_relaxed);
    while (linearPeak > currentPeak &&
           !state->peakLinear.compare_exchange_weak(
               currentPeak,
               linearPeak,
               std::memory_order_release,
               std::memory_order_relaxed)) {
    }
}

void captureCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount)
{
    Q_UNUSED(output);

    auto* state = static_cast<AudioRecorderWorkerState*>(device->pUserData);
    if (state == nullptr || input == nullptr) {
        return;
    }

    const ma_uint32 channelCount = device->capture.channels;
    const auto sampleCount = static_cast<std::size_t>(frameCount) * channelCount;
    const auto* inputSamples = static_cast<const qint16*>(input);
    const qint16 peakSample = peakSampleForBuffer(inputSamples, sampleCount);

    if (state->recording) {
        std::scoped_lock lock(state->bufferMutex);
        state->pendingSamples.insert(
            state->pendingSamples.end(),
            inputSamples,
            inputSamples + sampleCount);
    }

    publishPeak(state, static_cast<float>(peakSample) / AudioRecorderWorkerConfig::PeakScale);
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
    shutdownDevice();
    uninitializeContext();
    delete m_state;
}

bool AudioRecorderWorker::ensureContext()
{
    if (m_state->contextReady) {
        return true;
    }

    if (!AudioRecorderWorkerPlatform::supportsSpeakerCapture()) {
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

void AudioRecorderWorker::selectDevice(int deviceIndex)
{
    if (deviceIndex < 0) {
        shutdownDevice();
        emit levelChanged(AudioRecorderWorkerConfig::MinDb);
        return;
    }

    if (!startMonitoringDevice(deviceIndex)) {
        emit levelChanged(AudioRecorderWorkerConfig::MinDb);
    }
}

bool AudioRecorderWorker::flushPendingSamples(QString* errorMessage)
{
    std::vector<qint16> pendingSamples;
    {
        std::scoped_lock lock(m_state->bufferMutex);
        pendingSamples.swap(m_state->pendingSamples);
    }

    if (pendingSamples.empty()) {
        return true;
    }

    return m_writer->appendSamples(
        std::span<const qint16>(pendingSamples.data(), pendingSamples.size()),
        errorMessage);
}

bool AudioRecorderWorker::startMonitoringDevice(int deviceIndex)
{
    if (!ensureContext()) {
        return false;
    }

    if (deviceIndex < 0 || deviceIndex >= m_state->devices.size()) {
        emit errorOccurred(QStringLiteral("Select a valid playback device."));
        return false;
    }

    if (m_state->deviceReady && m_state->currentDeviceIndex == deviceIndex) {
        return true;
    }

    shutdownDevice();
    m_state->peakLinear.store(0.0f, std::memory_order_relaxed);

    const DeviceEntry& selectedDevice = m_state->devices[deviceIndex];
    ma_device_config deviceConfig = AudioRecorderWorkerPlatform::createDeviceConfig(selectedDevice);
    deviceConfig.dataCallback = captureCallback;
    deviceConfig.pUserData = m_state;

    const ma_result initResult = ma_device_init(
        &m_state->context,
        &deviceConfig,
        &m_state->device);
    if (initResult != MA_SUCCESS) {
        m_state->currentDeviceIndex = -1;
        emit errorOccurred(backendErrorMessage("Failed to initialize speaker capture", initResult));
        return false;
    }

    m_state->deviceReady = true;
    m_state->currentDeviceIndex = deviceIndex;
    m_state->sampleRate = m_state->device.sampleRate;
    m_state->channelCount = static_cast<quint16>(m_state->device.capture.channels);

    const ma_result startResult = ma_device_start(&m_state->device);
    if (startResult != MA_SUCCESS) {
        shutdownDevice();
        emit errorOccurred(backendErrorMessage("Failed to start speaker capture", startResult));
        return false;
    }

    return true;
}

void AudioRecorderWorker::startRecording(int deviceIndex, const QString& filePath)
{
    if (filePath.isEmpty()) {
        emit errorOccurred(QStringLiteral("No output path was selected."));
        return;
    }

    if (!startMonitoringDevice(deviceIndex)) {
        return;
    }

    {
        std::scoped_lock lock(m_state->bufferMutex);
        m_state->pendingSamples.clear();
    }

    QString errorMessage;
    if (!m_writer->open(filePath, m_state->sampleRate, m_state->channelCount, &errorMessage)) {
        emit errorOccurred(QStringLiteral("Failed to open WAV file: %1").arg(errorMessage));
        return;
    }

    m_state->peakLinear.store(0.0f, std::memory_order_relaxed);
    m_state->recording = true;
    emit recordingStateChanged(true);
}

void AudioRecorderWorker::stopRecording()
{
    shutdownDevice();
    emit levelChanged(AudioRecorderWorkerConfig::MinDb);

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
    {
        std::scoped_lock lock(m_state->bufferMutex);
        m_state->pendingSamples.clear();
    }

    m_state->recording = false;
    m_writer->discard();
}

void AudioRecorderWorker::publishLevel()
{
    if (m_state->recording) {
        QString errorMessage;
        if (!flushPendingSamples(&errorMessage)) {
            shutdownDevice();
            m_writer->discard();
            emit errorOccurred(QStringLiteral("Failed to write WAV file: %1").arg(errorMessage));
        }
    }

    const float peak = m_state->peakLinear.exchange(0.0f, std::memory_order_acq_rel);
    const float db = peak > AudioRecorderWorkerConfig::SilenceThreshold
        ? std::clamp(20.0f * std::log10(peak), AudioRecorderWorkerConfig::MinDb, 0.0f)
        : AudioRecorderWorkerConfig::MinDb;
    emit levelChanged(db);
}

void AudioRecorderWorker::shutdownDevice()
{
    if (!m_state->deviceReady) {
        m_state->recording = false;
        m_state->currentDeviceIndex = -1;
        emit recordingStateChanged(false);
        return;
    }

    m_state->recording = false;
    ma_device_uninit(&m_state->device);
    m_state->deviceReady = false;
    m_state->currentDeviceIndex = -1;
    emit recordingStateChanged(false);
}
