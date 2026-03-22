#include "MainWindowUi.h"
#include "LevelMeterWidget.h"
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QString defaultRecordingFileName()
{
    return QStringLiteral("speaker-capture.wav");
}

QString wavFileFilter()
{
    return QStringLiteral("WAV files (*.wav)");
}

QString emptyDeviceStatusText()
{
    return QStringLiteral("No playback devices were found.");
}

QString readyStatusText()
{
    return QStringLiteral("Monitoring selected output device. Press Record to save audio.");
}

}

void MainWindowUi::setup(QMainWindow* window)
{
    auto* central = new QWidget(window);
    auto* mainLayout = new QVBoxLayout(central);
    auto* deviceGroup = new QGroupBox(QStringLiteral("Speaker Loopback"), central);
    auto* deviceLayout = new QGridLayout(deviceGroup);

    auto* deviceLabel = new QLabel(QStringLiteral("Output device"), deviceGroup);
    deviceCombo = new QComboBox(deviceGroup);
    refreshButton = new QPushButton(QStringLiteral("Refresh"), deviceGroup);
    levelMeter = new LevelMeterWidget(deviceGroup);
    recordButton = new QPushButton(QStringLiteral("Record"), deviceGroup);
    stopButton = new QPushButton(QStringLiteral("Stop"), deviceGroup);
    m_statusLabel = new QLabel(QStringLiteral("Enumerating playback devices..."), central);

    deviceLayout->addWidget(deviceLabel, 0, 0);
    deviceLayout->addWidget(deviceCombo, 0, 1);
    deviceLayout->addWidget(refreshButton, 0, 2);
    deviceLayout->addWidget(new QLabel(QStringLiteral("Level"), deviceGroup), 1, 0);
    deviceLayout->addWidget(levelMeter, 1, 1, 1, 2);
    deviceLayout->addWidget(recordButton, 2, 1);
    deviceLayout->addWidget(stopButton, 2, 2);

    mainLayout->addWidget(deviceGroup);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addStretch(1);

    window->setCentralWidget(central);
    window->setWindowTitle(QStringLiteral("System Audio Recorder"));
    window->resize(560, 220);

    setRecordingState(false, false);
}

void MainWindowUi::setDevices(const QStringList& deviceNames, int defaultIndex)
{
    deviceCombo->blockSignals(true);
    deviceCombo->clear();
    deviceCombo->addItems(deviceNames);

    if (defaultIndex >= 0 && defaultIndex < deviceCombo->count()) {
        deviceCombo->setCurrentIndex(defaultIndex);
    } else if (deviceCombo->count() > 0) {
        deviceCombo->setCurrentIndex(0);
    }

    deviceCombo->blockSignals(false);
    setRecordingState(false, deviceCombo->count() > 0);
    setStatusText(deviceNames.isEmpty() ? emptyDeviceStatusText() : readyStatusText());
}

void MainWindowUi::setRecordingState(bool isRecording, bool hasDevices)
{
    recordButton->setEnabled(!isRecording && hasDevices);
    stopButton->setEnabled(isRecording);
    deviceCombo->setEnabled(!isRecording);
    refreshButton->setEnabled(!isRecording);
}

void MainWindowUi::setStatusText(const QString& text)
{
    m_statusLabel->setText(text);
}

int MainWindowUi::currentDeviceIndex() const
{
    return deviceCombo->currentIndex();
}

int MainWindowUi::deviceCount() const
{
    return deviceCombo->count();
}

QString MainWindowUi::requestSaveFilePath(QMainWindow* window) const
{
    return QFileDialog::getSaveFileName(
        window,
        QStringLiteral("Save Recording"),
        defaultRecordingFileName(),
        wavFileFilter());
}