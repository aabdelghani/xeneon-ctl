// xeneon-ctl — fullscreen touch visualizer for the Xeneon Edge (M5.1).
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A translucent always-on-top window placed on the Edge's screen. Every touch
// (up to the panel's 5 points) draws an animated ring + crosshair + coordinate
// label where the panel is pressed, so you can see exactly where touches land.
// Esc or the close button dismisses it.
#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QPointF>
#include <QWidget>

class QScreen;

namespace xen {

class TouchOverlay : public QWidget {
    Q_OBJECT
public:
    explicit TouchOverlay(QWidget* parent = nullptr);

    // Places the overlay on `screen` (the Edge) and shows it fullscreen.
    void showOnScreen(QScreen* screen);

protected:
    bool event(QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    struct Touch {
        QPointF pos;
        bool active = false;
        int ageMs = 0; // for release fade
    };

    void bump(int id, QPointF p, bool active);

    QHash<int, Touch> m_touches; // by touch id (mouse uses id -1)
    QElapsedTimer m_clock;
    int m_frames = 0;
};

} // namespace xen
