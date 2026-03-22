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

    m_ui->setSourceConfigurationChangedCallback([this]() {
        queueConfigureSources(
            m_ui->selectedDeviceIndices(),
            m_ui->mutedStates(),
            m_ui->gainPercents());
    });

    connect(m_ui->refreshButton, &QPushButton::clicked, this, &MainWindow::requestRefresh);
    connect(m_ui->recordButton, &QPushButton::clicked, this, &MainWindow::requestStartRecording);
    connect(m_ui->stopButton, &QPushButton::clicked, this, &MainWindow::requestStopRecording);

    connect(m_worker, &AudioRecorderWorker::devicesReady, this, &MainWindow::onDevicesReady);
    connect(
        m_worker,
        &AudioRecorderWorker::levelChanged,
        m_ui->levelMeter,
        &LevelMeterWidget::setLevelDb);
    connect(m_worker, &AudioRecorderWorker::sourceLevelsChanged, this, [this](const QVector<float>& sourceLevelsDb) {
        m_ui->setSourceLevels(sourceLevelsDb);
    });
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

void MainWindow::queueConfigureSources(
    const QVector<int>& deviceIndices,
    const QVector<bool>& mutedStates,
    const QVector<int>& gainPercents)
{
    QMetaObject::invokeMethod(
        m_worker,
        [this, deviceIndices, mutedStates, gainPercents]() {
            m_worker->configureSources(deviceIndices, mutedStates, gainPercents);
        },
        Qt::QueuedConnection);
}

void MainWindow::queueRefreshDevices()
{
    QMetaObject::invokeMethod(m_worker, &AudioRecorderWorker::refreshDevices, Qt::QueuedConnection);
}

void MainWindow::queueStartRecording(const QString& filePath)
{
    QMetaObject::invokeMethod(
        m_worker,
        [this, filePath]() {
            m_worker->startRecording(filePath);
        },
        Qt::QueuedConnection);
}

void MainWindow::queueStopRecording()
{
    QMetaObject::invokeMethod(m_worker, &AudioRecorderWorker::stopRecording, Qt::QueuedConnection);
}

void MainWindow::queueDiscardRecording()
{
    QMetaObject::invokeMethod(m_worker, &AudioRecorderWorker::discardRecording, Qt::QueuedConnection);
}

void MainWindow::requestRefresh()
{
    setStatusText(QStringLiteral("Refreshing audio devices..."));
    queueRefreshDevices();
}

void MainWindow::requestStartRecording()
{
    const QString filePath = m_ui->requestSaveFilePath(this);
    if (filePath.isEmpty()) {
        setStatusText(QStringLiteral("Recording canceled."));
        return;
    }

    setStatusText(QStringLiteral("Recording audio to %1").arg(filePath));
    queueStartRecording(filePath);
}

void MainWindow::requestStopRecording()
{
    setStatusText(QStringLiteral("Stopping capture..."));
    queueStopRecording();
}

void MainWindow::onDevicesReady(const QStringList& deviceNames, int defaultIndex)
{
    m_ui->setDevices(deviceNames, defaultIndex);
    queueConfigureSources(
        m_ui->selectedDeviceIndices(),
        m_ui->mutedStates(),
        m_ui->gainPercents());
}

void MainWindow::onRecordingStateChanged(bool isRecording)
{
    m_ui->setRecordingState(isRecording, hasDevices());

    if (isRecording) {
        return;
    }
}

void MainWindow::onRecordingStopped(bool hasAudio)
{
    if (!hasAudio) {
        setStatusText(QStringLiteral("Recording stopped. No audio was captured."));
    }
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