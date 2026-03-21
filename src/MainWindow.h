#pragma once

#include <QMainWindow>

#include <QString>
#include <QStringList>

class AudioRecorderWorker;
class QLabel;
class LevelMeterWidget;
class QComboBox;
class QPushButton;
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
    void setStatusText(const QString& text);

    QThread* m_audioThread = nullptr;
    AudioRecorderWorker* m_worker = nullptr;
    QComboBox* m_deviceCombo = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_recordButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    LevelMeterWidget* m_levelMeter = nullptr;
    QLabel* m_statusLabel = nullptr;
};
