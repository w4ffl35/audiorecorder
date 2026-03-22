#pragma once

#include "AudioRecorderWorkerConfig.h"
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>
#include <miniaudio.h>

class AudioRecorderWorkerState;

namespace AudioRecorderWorkerDetail {

enum class DeviceKind {
    Output,
    Input,
};

struct DeviceEntry
{
    QString name;
    ma_device_id playbackId{};
    ma_device_id captureId{};
    DeviceKind kind = DeviceKind::Output;
    bool isDefault = false;
};

struct CaptureEntry
{
    QString name;
    ma_device_id id{};
    bool isDefault = false;
};

struct EnumeratedDevices
{
    QVector<DeviceEntry> devices;
    QStringList names;
    int defaultIndex = -1;
    QString emptyMessage;
};

struct ActiveCaptureSource
{
    AudioRecorderWorkerState* owner = nullptr;
    DeviceEntry device;
    ma_device captureDevice{};
    std::mutex bufferMutex;
    std::deque<qint16> pendingSamples;
    std::atomic<float> peakLinear = 0.0f;
    std::atomic<float> gainLinear = 1.0f;
    std::atomic<bool> muted = false;
    bool deviceReady = false;
};

}

class AudioRecorderWorkerState
{
public:
    ma_context context{};
    QVector<AudioRecorderWorkerDetail::DeviceEntry> devices;
    std::vector<std::unique_ptr<AudioRecorderWorkerDetail::ActiveCaptureSource>> activeSources;
    bool contextReady = false;
    bool recording = false;
    quint32 sampleRate = AudioRecorderWorkerConfig::CaptureSampleRate;
    quint16 channelCount = AudioRecorderWorkerConfig::CaptureChannels;
};
