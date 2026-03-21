#pragma once

#include <QWidget>

class LevelMeterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LevelMeterWidget(QWidget* parent = nullptr);

    void setLevelDb(float levelDb);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    float m_levelDb = -60.0f;
};
