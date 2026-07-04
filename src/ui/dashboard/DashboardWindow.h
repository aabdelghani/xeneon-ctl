// xeneon-ctl — glanceable system dashboard rendered on the Edge (M6).
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A frameless fullscreen window sized to the Edge (2560x720). Draws a clock and
// CPU / GPU / RAM tiles from SensorSource, updated ~1 Hz. Custom-painted so the
// dense ultrawide layout stays crisp. Esc closes.
#pragma once

#include "core/SensorSource.h"

#include <QWidget>

class QScreen;
class QTimer;

namespace xen {

class DashboardWindow : public QWidget {
    Q_OBJECT
public:
    explicit DashboardWindow(QWidget* parent = nullptr);
    void showOnScreen(QScreen* screen);

protected:
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    static void drawTile(class QPainter& g, const QRectF& r, const QString& title,
                  const QString& big, const QString& sub, double pct, const QColor& accent);

    SensorSource* m_sensors = nullptr;
    SensorSnapshot m_snap;
    QTimer* m_clockTimer = nullptr;
    QString m_time;
    QString m_date;
};

} // namespace xen
