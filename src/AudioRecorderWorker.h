#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class AudioRecorderWorkerState;
class QTimer;

class AudioRecorderWorker : public QObject
{
    Q_OBJECT

public:
    explicit AudioRecorderWorker(QObject* parent = nullptr);
    ~AudioRecorderWorker() override;

public slots:
    void initialize();
    void refreshDevices();
    void selectDevice(int deviceIndex);
    void startRecording(int deviceIndex);
    void stopRecording();
    void saveRecording(const QString& filePath);
    void discardRecording();

signals:
    void devicesReady(const QStringList& deviceNames, int defaultIndex);
    void levelChanged(float levelDb);
    void recordingStateChanged(bool isRecording);
    void recordingStopped(bool hasAudio);
    void recordingSaved(const QString& filePath);
    void errorOccurred(const QString& message);

private slots:
    void publishLevel();

private:
    bool ensureContext();
    bool startMonitoringDevice(int deviceIndex);
    void uninitializeContext();
    void shutdownDevice();

    QTimer* m_levelTimer = nullptr;
    AudioRecorderWorkerState* m_state = nullptr;
};
