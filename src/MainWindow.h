#pragma once

#include <QMainWindow>
#include <QVector>
#include <QString>
#include <QStringList>
#include <memory>

class AudioRecorderWorker;
class MainWindowUi;
class QThread;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void requestRefresh();
    void requestStartRecording();
    void requestStopRecording();
    void onDevicesReady(const QStringList& deviceNames, int defaultIndex);
    void onRecordingStateChanged(bool isRecording);
    void onRecordingStopped(bool hasAudio);
    void onRecordingSaved(const QString& filePath);
    void showError(const QString& message);

private:
    void connectSignals();
    void startAudioThread();
    void stopAudioThread();
    void queueConfigureSources(
        const QVector<int>& deviceIndices,
        const QVector<bool>& mutedStates,
        const QVector<int>& gainPercents);
    void queueRefreshDevices();
    void queueStartRecording(const QString& filePath);
    void queueStopRecording();
    void queueDiscardRecording();
    void setStatusText(const QString& text);
    bool hasDevices() const;

    QThread* m_audioThread = nullptr;
    AudioRecorderWorker* m_worker = nullptr;
    std::unique_ptr<MainWindowUi> m_ui;
};