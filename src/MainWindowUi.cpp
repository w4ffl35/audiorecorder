#include "MainWindowUi.h"
#include "LevelMeterWidget.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QString defaultRecordingFileName()
{
    return QStringLiteral("audio-capture.wav");
}

QString wavFileFilter()
{
    return QStringLiteral("WAV files (*.wav)");
}

QString emptyDeviceStatusText()
{
    return QStringLiteral("No audio devices were found.");
}

QString readyStatusText()
{
    return QStringLiteral("Configure one or more audio sources, then press Record to choose a WAV file and start recording.");
}

}

void MainWindowUi::setup(QMainWindow* window)
{
    auto* central = new QWidget(window);
    auto* mainLayout = new QVBoxLayout(central);
    auto* deviceGroup = new QGroupBox(QStringLiteral("Audio Sources"), central);
    auto* deviceLayout = new QVBoxLayout(deviceGroup);

    refreshButton = new QPushButton(QStringLiteral("Refresh"), deviceGroup);
    m_addSourceButton = new QPushButton(QStringLiteral("Add Source"), deviceGroup);
    levelMeter = new LevelMeterWidget(deviceGroup);
    recordButton = new QPushButton(QStringLiteral("Record"), deviceGroup);
    stopButton = new QPushButton(QStringLiteral("Stop"), deviceGroup);
    m_statusLabel = new QLabel(QStringLiteral("Enumerating audio devices..."), central);

    auto* sourceToolbar = new QHBoxLayout();
    sourceToolbar->addWidget(new QLabel(QStringLiteral("Sources"), deviceGroup));
    sourceToolbar->addStretch(1);
    sourceToolbar->addWidget(m_addSourceButton);
    sourceToolbar->addWidget(refreshButton);

    m_sourceRowsLayout = new QVBoxLayout();
    m_sourceRowsLayout->setContentsMargins(0, 0, 0, 0);
    m_sourceRowsLayout->setSpacing(8);

    auto* levelRow = new QHBoxLayout();
    levelRow->addWidget(new QLabel(QStringLiteral("Level"), deviceGroup));
    levelRow->addWidget(levelMeter, 1);

    auto* actionRow = new QHBoxLayout();
    actionRow->addStretch(1);
    actionRow->addWidget(recordButton);
    actionRow->addWidget(stopButton);

    deviceLayout->addLayout(sourceToolbar);
    deviceLayout->addLayout(m_sourceRowsLayout);
    deviceLayout->addLayout(levelRow);
    deviceLayout->addLayout(actionRow);

    mainLayout->addWidget(deviceGroup);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addStretch(1);

    window->setCentralWidget(central);
    window->setWindowTitle(QStringLiteral("System Audio Recorder"));
    window->resize(760, 280);

    QObject::connect(m_addSourceButton, &QPushButton::clicked, window, [this]() {
        addSourceRow(m_defaultDeviceIndex, false);
        notifySourceConfigurationChanged();
    });

    addSourceRow();

    setRecordingState(false, false);
}

void MainWindowUi::setDevices(const QStringList& deviceNames, int defaultIndex)
{
    m_deviceNames = deviceNames;
    m_defaultDeviceIndex = defaultIndex;
    repopulateSourceRows();
    setRecordingState(m_isRecording, !m_deviceNames.isEmpty());
    setStatusText(deviceNames.isEmpty() ? emptyDeviceStatusText() : readyStatusText());
}

void MainWindowUi::setRecordingState(bool isRecording, bool hasDevices)
{
    m_isRecording = isRecording;
    recordButton->setEnabled(!isRecording && hasDevices);
    stopButton->setEnabled(isRecording);
    refreshButton->setEnabled(!isRecording);
    updateAddButton();

    for (SourceRow& row : m_sourceRows) {
        row.deviceCombo->setEnabled(!isRecording);
        row.removeButton->setEnabled(!isRecording && m_sourceRows.size() > 1);
    }

    updateRemoveButtons();
}

void MainWindowUi::setStatusText(const QString& text)
{
    m_statusLabel->setText(text);
}

void MainWindowUi::setSourceLevels(const QVector<float>& sourceLevelsDb)
{
    for (int index = 0; index < m_sourceRows.size(); ++index) {
        m_sourceRows[index].levelMeter->setLevelDb(sourceLevelsDb.value(index, -60.0f));
    }
}

int MainWindowUi::deviceCount() const
{
    return m_deviceNames.size();
}

QVector<int> MainWindowUi::selectedDeviceIndices() const
{
    QVector<int> indices;
    indices.reserve(m_sourceRows.size());

    for (const SourceRow& row : m_sourceRows) {
        if (row.deviceCombo->currentIndex() >= 0) {
            indices.push_back(row.deviceCombo->currentData().toInt());
        }
    }

    return indices;
}

QVector<bool> MainWindowUi::mutedStates() const
{
    QVector<bool> mutedStates;
    mutedStates.reserve(m_sourceRows.size());

    for (const SourceRow& row : m_sourceRows) {
        mutedStates.push_back(row.muteCheckBox->isChecked());
    }

    return mutedStates;
}

QVector<int> MainWindowUi::gainPercents() const
{
    QVector<int> gains;
    gains.reserve(m_sourceRows.size());

    for (const SourceRow& row : m_sourceRows) {
        gains.push_back(row.gainSlider->value());
    }

    return gains;
}

QString MainWindowUi::requestSaveFilePath(QMainWindow* window) const
{
    return QFileDialog::getSaveFileName(
        window,
        QStringLiteral("Save Recording"),
        defaultRecordingFileName(),
        wavFileFilter());
}

void MainWindowUi::setSourceConfigurationChangedCallback(std::function<void()> callback)
{
    m_sourceConfigurationChangedCallback = std::move(callback);
}

void MainWindowUi::addSourceRow(int selectedIndex, bool muted, int gainPercent)
{
    SourceRow row;
    row.container = new QWidget();
    auto* rowLayout = new QVBoxLayout(row.container);
    rowLayout->setContentsMargins(0, 0, 0, 0);

    auto* topRow = new QHBoxLayout();
    auto* meterRow = new QHBoxLayout();

    row.label = new QLabel(row.container);
    row.deviceCombo = new QComboBox(row.container);
    row.muteCheckBox = new QCheckBox(QStringLiteral("Mute"), row.container);
    row.gainSlider = new QSlider(Qt::Horizontal, row.container);
    row.gainLabel = new QLabel(row.container);
    row.levelMeter = new LevelMeterWidget(row.container);
    row.removeButton = new QPushButton(QStringLiteral("Remove"), row.container);

    row.gainSlider->setRange(0, 200);
    row.gainSlider->setValue(gainPercent);

    topRow->addWidget(row.label);
    topRow->addWidget(row.deviceCombo, 1);
    topRow->addWidget(row.muteCheckBox);
    topRow->addWidget(row.removeButton);

    meterRow->addWidget(new QLabel(QStringLiteral("Level"), row.container));
    meterRow->addWidget(row.levelMeter, 1);
    meterRow->addWidget(new QLabel(QStringLiteral("Gain"), row.container));
    meterRow->addWidget(row.gainSlider, 1);
    meterRow->addWidget(row.gainLabel);

    rowLayout->addLayout(topRow);
    rowLayout->addLayout(meterRow);

    row.muteCheckBox->setChecked(muted);
    row.levelMeter->setLevelDb(-60.0f);

    QObject::connect(row.deviceCombo, &QComboBox::currentIndexChanged, row.container, [this]() {
        refreshSourceOptions();
        notifySourceConfigurationChanged();
    });
    QObject::connect(row.muteCheckBox, &QCheckBox::toggled, row.container, [this]() {
        notifySourceConfigurationChanged();
    });
    QObject::connect(row.gainSlider, &QSlider::valueChanged, row.container, [this]() {
        updateGainLabels();
        notifySourceConfigurationChanged();
    });
    QObject::connect(row.removeButton, &QPushButton::clicked, row.container, [this, container = row.container]() {
        for (int index = 0; index < m_sourceRows.size(); ++index) {
            if (m_sourceRows[index].container == container) {
                removeSourceRow(index);
                notifySourceConfigurationChanged();
                return;
            }
        }
    });

    row.deviceCombo->setEnabled(!m_isRecording);

    m_sourceRows.push_back(row);
    m_sourceRowsLayout->addWidget(row.container);
    updateSourceLabels();
    updateGainLabels();
    refreshSourceOptions();
    updateAddButton();
    updateRemoveButtons();
}

void MainWindowUi::removeSourceRow(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= m_sourceRows.size() || m_sourceRows.size() <= 1) {
        return;
    }

    SourceRow row = m_sourceRows[rowIndex];
    m_sourceRows.removeAt(rowIndex);
    delete row.container;
    updateSourceLabels();
    refreshSourceOptions();
    updateAddButton();
    updateRemoveButtons();
}

void MainWindowUi::repopulateSourceRows()
{
    const QVector<int> previousSelections = selectedDeviceIndices();
    const QVector<bool> previousMutedStates = mutedStates();
    const QVector<int> previousGains = gainPercents();

    for (const SourceRow& row : m_sourceRows) {
        delete row.container;
    }
    m_sourceRows.clear();

    const int rowCount = std::max(1, static_cast<int>(previousSelections.size()));
    for (int index = 0; index < rowCount; ++index) {
        addSourceRow(
            previousSelections.value(index, m_defaultDeviceIndex),
            previousMutedStates.value(index, false),
            previousGains.value(index, 100));
    }

    notifySourceConfigurationChanged();
}

void MainWindowUi::refreshSourceOptions()
{
    const QVector<int> currentSelections = selectedDeviceIndices();

    for (int rowIndex = 0; rowIndex < m_sourceRows.size(); ++rowIndex) {
        SourceRow& row = m_sourceRows[rowIndex];
        const QSet<int> excludedIndices = selectedDeviceIndexSet(rowIndex);
        const int desiredSelection = effectiveSelectionIndex(currentSelections.value(rowIndex, -1), excludedIndices);

        const QSignalBlocker blocker(row.deviceCombo);
        row.deviceCombo->clear();

        for (int deviceIndex = 0; deviceIndex < m_deviceNames.size(); ++deviceIndex) {
            if (excludedIndices.contains(deviceIndex) && deviceIndex != desiredSelection) {
                continue;
            }

            row.deviceCombo->addItem(m_deviceNames[deviceIndex], deviceIndex);
        }

        const int comboIndex = row.deviceCombo->findData(desiredSelection);
        row.deviceCombo->setCurrentIndex(comboIndex);
    }
}

void MainWindowUi::updateSourceLabels()
{
    for (int index = 0; index < m_sourceRows.size(); ++index) {
        m_sourceRows[index].label->setText(QStringLiteral("Source %1").arg(index + 1));
    }
}

void MainWindowUi::updateRemoveButtons()
{
    const bool canRemove = !m_isRecording && m_sourceRows.size() > 1;
    for (SourceRow& row : m_sourceRows) {
        row.removeButton->setEnabled(canRemove);
    }
}

void MainWindowUi::updateAddButton()
{
    const bool hasUnusedDevice = m_sourceRows.size() < m_deviceNames.size();
    m_addSourceButton->setEnabled(!m_isRecording && !m_deviceNames.isEmpty() && hasUnusedDevice);
}

void MainWindowUi::updateGainLabels()
{
    for (SourceRow& row : m_sourceRows) {
        row.gainLabel->setText(QStringLiteral("%1%").arg(row.gainSlider->value()));
    }
}

void MainWindowUi::notifySourceConfigurationChanged() const
{
    if (m_sourceConfigurationChangedCallback) {
        m_sourceConfigurationChangedCallback();
    }
}

int MainWindowUi::effectiveSelectionIndex(int requestedIndex, const QSet<int>& excludedIndices) const
{
    if (m_deviceNames.isEmpty()) {
        return -1;
    }

    if (requestedIndex >= 0 && requestedIndex < m_deviceNames.size() && !excludedIndices.contains(requestedIndex)) {
        return requestedIndex;
    }

    if (m_defaultDeviceIndex >= 0 && m_defaultDeviceIndex < m_deviceNames.size() &&
        !excludedIndices.contains(m_defaultDeviceIndex)) {
        return m_defaultDeviceIndex;
    }

    for (int index = 0; index < m_deviceNames.size(); ++index) {
        if (!excludedIndices.contains(index)) {
            return index;
        }
    }

    return -1;
}

QSet<int> MainWindowUi::selectedDeviceIndexSet(int skipRow) const
{
    QSet<int> selectedIndices;

    for (int rowIndex = 0; rowIndex < m_sourceRows.size(); ++rowIndex) {
        if (rowIndex == skipRow) {
            continue;
        }

        if (m_sourceRows[rowIndex].deviceCombo->currentIndex() < 0) {
            continue;
        }

        const int deviceIndex = m_sourceRows[rowIndex].deviceCombo->currentData().toInt();
        if (deviceIndex >= 0) {
            selectedIndices.insert(deviceIndex);
        }
    }

    return selectedIndices;
}