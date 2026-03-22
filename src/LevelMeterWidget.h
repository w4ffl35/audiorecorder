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
    static constexpr int MinimumMeterHeight = 30;
    static constexpr float MinDb = -60.0f;
    static constexpr float MaxDb = 0.0f;
    static constexpr qreal FrameInset = 1.0;
    static constexpr qreal FillInset = 4.0;
    static constexpr qreal FrameRadius = 8.0;
    static constexpr qreal FillRadius = 6.0;
    static constexpr qreal TextPadding = 10.0;

    float m_levelDb = MinDb;
};
