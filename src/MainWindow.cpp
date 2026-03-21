#include "MainWindow.h"

#include "AudioRecorderWorker.h"
#include "LevelMeterWidget.h"

#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_audioThread(new QThread(this))
    , m_worker(new AudioRecorderWorker)
{
    auto* central = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);
    auto* deviceGroup = new QGroupBox(QStringLiteral("Speaker Loopback"), central);
    auto* deviceLayout = new QGridLayout(deviceGroup);

    auto* deviceLabel = new QLabel(QStringLiteral("Output device"), deviceGroup);
    m_deviceCombo = new QComboBox(deviceGroup);
    m_refreshButton = new QPushButton(QStringLiteral("Refresh"), deviceGroup);
    m_levelMeter = new LevelMeterWidget(deviceGroup);
    m_recordButton = new QPushButton(QStringLiteral("Record"), deviceGroup);
    m_stopButton = new QPushButton(QStringLiteral("Stop"), deviceGroup);
    m_statusLabel = new QLabel(QStringLiteral("Enumerating playback devices..."), central);

    deviceLayout->addWidget(deviceLabel, 0, 0);
    deviceLayout->addWidget(m_deviceCombo, 0, 1);
    deviceLayout->addWidget(m_refreshButton, 0, 2);
    deviceLayout->addWidget(new QLabel(QStringLiteral("Level"), deviceGroup), 1, 0);
    deviceLayout->addWidget(m_levelMeter, 1, 1, 1, 2);
    deviceLayout->addWidget(m_recordButton, 2, 1);
    deviceLayout->addWidget(m_stopButton, 2, 2);

    mainLayout->addWidget(deviceGroup);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addStretch(1);

    setCentralWidget(central);
    setWindowTitle(QStringLiteral("System Audio Recorder"));
    resize(560, 220);

    m_recordButton->setEnabled(false);
    m_stopButton->setEnabled(false);

    m_worker->moveToThread(m_audioThread);
    connect(m_audioThread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::requestRefresh);
    connect(m_deviceCombo, &QComboBox::currentIndexChanged, this, &MainWindow::requestDeviceSelection);
    connect(m_recordButton, &QPushButton::clicked, this, &MainWindow::requestStartRecording);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::requestStopRecording);

    connect(m_worker, &AudioRecorderWorker::devicesReady, this, &MainWindow::onDevicesReady);
    connect(m_worker, &AudioRecorderWorker::levelChanged, m_levelMeter, &LevelMeterWidget::setLevelDb);
    connect(m_worker, &AudioRecorderWorker::recordingStateChanged, this, &MainWindow::onRecordingStateChanged);
    connect(m_worker, &AudioRecorderWorker::recordingStopped, this, &MainWindow::onRecordingStopped);
    connect(m_worker, &AudioRecorderWorker::recordingSaved, this, &MainWindow::onRecordingSaved);
    connect(m_worker, &AudioRecorderWorker::errorOccurred, this, &MainWindow::showError);

    m_audioThread->start();
    QMetaObject::invokeMethod(m_worker, &AudioRecorderWorker::initialize, Qt::QueuedConnection);
}

MainWindow::~MainWindow()
{
    disconnect(m_worker, nullptr, this, nullptr);
    QMetaObject::invokeMethod(m_worker, &AudioRecorderWorker::discardRecording, Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_worker, &AudioRecorderWorker::stopRecording, Qt::BlockingQueuedConnection);
    m_audioThread->quit();
    m_audioThread->wait();
}

void MainWindow::requestRefresh()
{
    setStatusText(QStringLiteral("Refreshing playback devices..."));
    QMetaObject::invokeMethod(m_worker, &AudioRecorderWorker::refreshDevices, Qt::QueuedConnection);
}

void MainWindow::requestStartRecording()
{
    setStatusText(QStringLiteral("Starting loopback capture..."));
    const int deviceIndex = m_deviceCombo->currentIndex();
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, deviceIndex]() {
            worker->startRecording(deviceIndex);
        },
        Qt::QueuedConnection);
}

void MainWindow::requestDeviceSelection(int deviceIndex)
{
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, deviceIndex]() {
            worker->selectDevice(deviceIndex);
        },
        Qt::QueuedConnection);
}

void MainWindow::requestStopRecording()
{
    setStatusText(QStringLiteral("Stopping capture..."));
    QMetaObject::invokeMethod(m_worker, &AudioRecorderWorker::stopRecording, Qt::QueuedConnection);
}

void MainWindow::onDevicesReady(const QStringList& deviceNames, int defaultIndex)
{
    m_deviceCombo->blockSignals(true);
    m_deviceCombo->clear();
    m_deviceCombo->addItems(deviceNames);

    if (defaultIndex >= 0 && defaultIndex < m_deviceCombo->count()) {
        m_deviceCombo->setCurrentIndex(defaultIndex);
    } else if (m_deviceCombo->count() > 0) {
        m_deviceCombo->setCurrentIndex(0);
    }
    m_deviceCombo->blockSignals(false);

    m_recordButton->setEnabled(m_deviceCombo->count() > 0);
    m_stopButton->setEnabled(false);
    m_deviceCombo->setEnabled(true);
    m_refreshButton->setEnabled(true);

    setStatusText(deviceNames.isEmpty()
                      ? QStringLiteral("No playback devices were found.")
                      : QStringLiteral("Monitoring selected output device. Press Record to save audio."));

    if (m_deviceCombo->count() > 0) {
        requestDeviceSelection(m_deviceCombo->currentIndex());
    }
}

void MainWindow::onRecordingStateChanged(bool isRecording)
{
    m_recordButton->setEnabled(!isRecording && m_deviceCombo->count() > 0);
    m_stopButton->setEnabled(isRecording);
    m_deviceCombo->setEnabled(!isRecording);
    m_refreshButton->setEnabled(!isRecording);

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

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Save Recording"),
        QStringLiteral("speaker-capture.wav"),
        QStringLiteral("WAV files (*.wav)"));

    if (filePath.isEmpty()) {
        QMetaObject::invokeMethod(m_worker, &AudioRecorderWorker::discardRecording, Qt::QueuedConnection);
        setStatusText(QStringLiteral("Recording discarded."));
        return;
    }

    setStatusText(QStringLiteral("Saving WAV file..."));
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, filePath]() {
            worker->saveRecording(filePath);
        },
        Qt::QueuedConnection);
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

void MainWindow::setStatusText(const QString& text)
{
    m_statusLabel->setText(text);
}
