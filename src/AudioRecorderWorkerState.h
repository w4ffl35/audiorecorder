#pragma once

#include "AudioRecorderWorkerConfig.h"
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>
#include <atomic>
#include <mutex>
#include <vector>
#include <miniaudio.h>

namespace AudioRecorderWorkerDetail {

struct DeviceEntry
{
    QString name;
    ma_device_id playbackId{};
    ma_device_id captureId{};
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

}

class AudioRecorderWorkerState
{
public:
    ma_context context{};
    ma_device device{};
    QVector<AudioRecorderWorkerDetail::DeviceEntry> devices;
    std::vector<qint16> capturedSamples;
    std::mutex bufferMutex;
    std::atomic<float> peakLinear = 0.0f;
    bool contextReady = false;
    bool deviceReady = false;
    bool recording = false;
    int currentDeviceIndex = -1;
    quint32 sampleRate = AudioRecorderWorkerConfig::CaptureSampleRate;
    quint16 channelCount = AudioRecorderWorkerConfig::CaptureChannels;
};
