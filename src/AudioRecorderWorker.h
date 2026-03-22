#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>

class AudioRecorderWorkerState;
class QTimer;
namespace WavWriterDetail {
class WavWriterStream;
}

class AudioRecorderWorker : public QObject
{
    Q_OBJECT

public:
    explicit AudioRecorderWorker(QObject* parent = nullptr);
    ~AudioRecorderWorker() override;

public slots:
    void initialize();
    void refreshDevices();
    void configureSources(
        const QVector<int>& deviceIndices,
        const QVector<bool>& mutedStates,
        const QVector<int>& gainPercents);
    void startRecording(const QString& filePath);
    void stopRecording();
    void discardRecording();

signals:
    void devicesReady(const QStringList& deviceNames, int defaultIndex);
    void levelChanged(float levelDb);
    void sourceLevelsChanged(const QVector<float>& sourceLevelsDb);
    void recordingStateChanged(bool isRecording);
    void recordingStopped(bool hasAudio);
    void recordingSaved(const QString& filePath);
    void errorOccurred(const QString& message);

private slots:
    void publishLevel();

private:
    bool ensureContext();
    bool flushPendingSamples(QString* errorMessage = nullptr);
    bool startCaptureSources(
        const QVector<int>& deviceIndices,
        const QVector<bool>& mutedStates,
        const QVector<int>& gainPercents);
    bool updateActiveSourceMix(
        const QVector<int>& deviceIndices,
        const QVector<bool>& mutedStates,
        const QVector<int>& gainPercents);
    void uninitializeContext();
    void shutdownDevices();

    QTimer* m_levelTimer = nullptr;
    AudioRecorderWorkerState* m_state = nullptr;
    std::unique_ptr<WavWriterDetail::WavWriterStream> m_writer;
};
