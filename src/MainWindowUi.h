#pragma once

#include <QSet>
#include <QVector>

#include <functional>
#include <QString>
#include <QStringList>

class LevelMeterWidget;
class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QMainWindow;
class QPushButton;
class QSlider;
class QGridLayout;
class QGroupBox;
class QLayout;
class QVBoxLayout;
class QWidget;

class MainWindowUi
{
public:
    void setup(QMainWindow* window);
    void setDevices(const QStringList& deviceNames, int defaultIndex);
    void setRecordingState(bool isRecording, bool hasDevices);
    void setStatusText(const QString& text);
    void setSourceLevels(const QVector<float>& sourceLevelsDb);
    int deviceCount() const;
    QVector<int> selectedDeviceIndices() const;
    QVector<bool> mutedStates() const;
    QVector<int> gainPercents() const;
    QString requestSaveFilePath(QMainWindow* window) const;
    void setSourceConfigurationChangedCallback(std::function<void()> callback);

    QPushButton* refreshButton = nullptr;
    QPushButton* recordButton = nullptr;
    QPushButton* stopButton = nullptr;
    LevelMeterWidget* levelMeter = nullptr;

private:
    struct SourceRow {
        QWidget* container = nullptr;
        QLabel* label = nullptr;
        QComboBox* deviceCombo = nullptr;
        QCheckBox* muteCheckBox = nullptr;
        QSlider* gainSlider = nullptr;
        QLabel* gainLabel = nullptr;
        LevelMeterWidget* levelMeter = nullptr;
        QPushButton* removeButton = nullptr;
    };

    QWidget* createCentralWidget(QMainWindow* window) const;
    QVBoxLayout* createMainLayout(QWidget* centralWidget) const;
    QGroupBox* createDeviceGroup(QWidget* parent) const;
    QVBoxLayout* createDeviceLayout(QGroupBox* deviceGroup) const;
    void createDeviceControls(QGroupBox* deviceGroup);
    void populateDeviceLayout(QVBoxLayout* deviceLayout, QGroupBox* deviceGroup);
    void configureWindow(QMainWindow* window, QWidget* centralWidget);
    void addSourceRow(int selectedIndex = -1, bool muted = false, int gainPercent = 100);
    void removeSourceRow(int rowIndex);
    void repopulateSourceRows();
    void refreshSourceOptions();
    void updateSourceLabels();
    void updateRemoveButtons();
    void updateAddButton();
    void updateGainLabels();
    void notifySourceConfigurationChanged() const;
    int effectiveSelectionIndex(int requestedIndex, const QSet<int>& excludedIndices = {}) const;
    QSet<int> selectedDeviceIndexSet(int skipRow = -1) const;

    QLabel* m_statusLabel = nullptr;
    QPushButton* m_addSourceButton = nullptr;
    QVBoxLayout* m_sourceRowsLayout = nullptr;
    QStringList m_deviceNames;
    int m_defaultDeviceIndex = -1;
    bool m_isRecording = false;
    QVector<SourceRow> m_sourceRows;
    std::function<void()> m_sourceConfigurationChangedCallback;
};