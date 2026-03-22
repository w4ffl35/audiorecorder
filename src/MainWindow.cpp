#include "MainWindow.h"
#include "AudioRecorderWorker.h"
#include "LevelMeterWidget.h"
#include "MainWindowUi.h"
#include <QComboBox>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QThread>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_audioThread(new QThread(this))
    , m_worker(new AudioRecorderWorker)
    , m_ui(std::make_unique<MainWindowUi>())
{
    m_ui->setup(this);
    connectSignals();
    startAudioThread();
}

MainWindow::~MainWindow()
{
    stopAudioThread();
}

void MainWindow::connectSignals()
{
    m_worker->moveToThread(m_audioThread);
    connect(m_audioThread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_ui->refreshButton, &QPushButton::clicked, this, &MainWindow::requestRefresh);
    connect(
        m_ui->deviceCombo,
        &QComboBox::currentIndexChanged,
        this,
        &MainWindow::requestDeviceSelection);
    connect(m_ui->recordButton, &QPushButton::clicked, this, &MainWindow::requestStartRecording);
    connect(m_ui->stopButton, &QPushButton::clicked, this, &MainWindow::requestStopRecording);

    connect(m_worker, &AudioRecorderWorker::devicesReady, this, &MainWindow::onDevicesReady);
    connect(
        m_worker,
        &AudioRecorderWorker::levelChanged,
        m_ui->levelMeter,
        &LevelMeterWidget::setLevelDb);
    connect(
        m_worker,
        &AudioRecorderWorker::recordingStateChanged,
        this,
        &MainWindow::onRecordingStateChanged);
    connect(m_worker, &AudioRecorderWorker::recordingStopped, this, &MainWindow::onRecordingStopped);
    connect(m_worker, &AudioRecorderWorker::recordingSaved, this, &MainWindow::onRecordingSaved);
    connect(m_worker, &AudioRecorderWorker::errorOccurred, this, &MainWindow::showError);
}

void MainWindow::startAudioThread()
{
    m_audioThread->start();
    QMetaObject::invokeMethod(m_worker, &AudioRecorderWorker::initialize, Qt::QueuedConnection);
}

void MainWindow::stopAudioThread()
{
    disconnect(m_worker, nullptr, this, nullptr);
    QMetaObject::invokeMethod(m_worker, &AudioRecorderWorker::discardRecording, Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_worker, &AudioRecorderWorker::stopRecording, Qt::BlockingQueuedConnection);
    m_audioThread->quit();
    m_audioThread->wait();
}

void MainWindow::queueRefreshDevices()
{
    QMetaObject::invokeMethod(m_worker, &AudioRecorderWorker::refreshDevices, Qt::QueuedConnection);
}

void MainWindow::queueSelectDevice(int deviceIndex)
{
    QMetaObject::invokeMethod(m_worker, "selectDevice", Qt::QueuedConnection, Q_ARG(int, deviceIndex));
}

void MainWindow::queueStartRecording(int deviceIndex)
{
    QMetaObject::invokeMethod(m_worker, "startRecording", Qt::QueuedConnection, Q_ARG(int, deviceIndex));
}

void MainWindow::queueStopRecording()
{
    QMetaObject::invokeMethod(m_worker, &AudioRecorderWorker::stopRecording, Qt::QueuedConnection);
}

void MainWindow::queueSaveRecording(const QString& filePath)
{
    QMetaObject::invokeMethod(m_worker, "saveRecording", Qt::QueuedConnection, Q_ARG(QString, filePath));
}

void MainWindow::queueDiscardRecording()
{
    QMetaObject::invokeMethod(m_worker, &AudioRecorderWorker::discardRecording, Qt::QueuedConnection);
}

void MainWindow::requestRefresh()
{
    setStatusText(QStringLiteral("Refreshing playback devices..."));
    queueRefreshDevices();
}

void MainWindow::requestStartRecording()
{
    setStatusText(QStringLiteral("Starting loopback capture..."));
    queueStartRecording(m_ui->currentDeviceIndex());
}

void MainWindow::requestDeviceSelection(int deviceIndex)
{
    queueSelectDevice(deviceIndex);
}

void MainWindow::requestStopRecording()
{
    setStatusText(QStringLiteral("Stopping capture..."));
    queueStopRecording();
}

void MainWindow::onDevicesReady(const QStringList& deviceNames, int defaultIndex)
{
    m_ui->setDevices(deviceNames, defaultIndex);

    if (hasDevices()) {
        queueSelectDevice(m_ui->currentDeviceIndex());
    }
}

void MainWindow::onRecordingStateChanged(bool isRecording)
{
    m_ui->setRecordingState(isRecording, hasDevices());

    if (isRecording) {
        setStatusText(QStringLiteral("Recording speaker output to memory..."));
    }
}

void MainWindow::onRecordingStopped(bool hasAudio)
{
    if (!hasAudio) {
        setStatusText(QStringLiteral("Recording stopped. No audio was captured."));
        return;
    }

    const QString filePath = m_ui->requestSaveFilePath(this);
    if (filePath.isEmpty()) {
        queueDiscardRecording();
        setStatusText(QStringLiteral("Recording discarded."));
        return;
    }

    setStatusText(QStringLiteral("Saving WAV file..."));
    queueSaveRecording(filePath);
}

void MainWindow::onRecordingSaved(const QString& filePath)
{
    setStatusText(QStringLiteral("Saved recording to %1").arg(filePath));
}

void MainWindow::showError(const QString& message)
{
    setStatusText(message);
    QMessageBox::critical(this, QStringLiteral("Audio Recorder"), message);
}

bool MainWindow::hasDevices() const
{
    return m_ui->deviceCount() > 0;
}

void MainWindow::setStatusText(const QString& text)
{
    m_ui->setStatusText(text);
}