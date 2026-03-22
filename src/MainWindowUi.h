#pragma once

#include <QString>
#include <QStringList>

class LevelMeterWidget;
class QComboBox;
class QLabel;
class QMainWindow;
class QPushButton;
class QGridLayout;
class QGroupBox;
class QVBoxLayout;
class QWidget;

class MainWindowUi
{
public:
    void setup(QMainWindow* window);
    void setDevices(const QStringList& deviceNames, int defaultIndex);
    void setRecordingState(bool isRecording, bool hasDevices);
    void setStatusText(const QString& text);
    int currentDeviceIndex() const;
    int deviceCount() const;
    QString requestSaveFilePath(QMainWindow* window) const;

    QComboBox* deviceCombo = nullptr;
    QPushButton* refreshButton = nullptr;
    QPushButton* recordButton = nullptr;
    QPushButton* stopButton = nullptr;
    LevelMeterWidget* levelMeter = nullptr;

private:
    QWidget* createCentralWidget(QMainWindow* window) const;
    QVBoxLayout* createMainLayout(QWidget* centralWidget) const;
    QGroupBox* createDeviceGroup(QWidget* parent) const;
    QGridLayout* createDeviceLayout(QGroupBox* deviceGroup) const;
    void createDeviceControls(QGroupBox* deviceGroup);
    void populateDeviceLayout(QGridLayout* deviceLayout, QGroupBox* deviceGroup);
    void configureWindow(QMainWindow* window, QWidget* centralWidget);
    void applyDefaultDeviceSelection(int defaultIndex);

    QLabel* m_statusLabel = nullptr;
};