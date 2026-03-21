#include "LevelMeterWidget.h"

#include <QLinearGradient>
#include <QPaintEvent>
#include <QPainter>

#include <algorithm>

LevelMeterWidget::LevelMeterWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(30);
}

void LevelMeterWidget::setLevelDb(float levelDb)
{
    const float clamped = std::clamp(levelDb, -60.0f, 0.0f);
    if (qFuzzyCompare(m_levelDb, clamped)) {
        return;
    }

    m_levelDb = clamped;
    update();
}

void LevelMeterWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF frame = rect().adjusted(1.0, 1.0, -1.0, -1.0);
    painter.setPen(QColor(40, 40, 40));
    painter.setBrush(QColor(20, 20, 20));
    painter.drawRoundedRect(frame, 8.0, 8.0);

    const float normalized = (m_levelDb + 60.0f) / 60.0f;
    QRectF fill = frame.adjusted(4.0, 4.0, -4.0, -4.0);
    fill.setWidth(fill.width() * std::clamp(normalized, 0.0f, 1.0f));

    QLinearGradient gradient(fill.topLeft(), fill.topRight());
    gradient.setColorAt(0.0, QColor(0, 180, 120));
    gradient.setColorAt(0.6, QColor(220, 180, 40));
    gradient.setColorAt(1.0, QColor(220, 70, 40));

    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawRoundedRect(fill, 6.0, 6.0);

    painter.setPen(QColor(235, 235, 235));
    painter.drawText(frame.adjusted(10.0, 0.0, -10.0, 0.0), Qt::AlignVCenter | Qt::AlignRight,
                     QStringLiteral("%1 dB").arg(m_levelDb, 0, 'f', 1));
}
