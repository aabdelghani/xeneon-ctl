// xeneon-ctl — persistent, click-through touch ripple overlay for the Edge.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fully transparent, input-transparent window pinned on the Edge. It never
// receives input itself; it is driven by TouchEventSource signals and just
// paints a ripple where each touch lands. No cursor, no dimming.
#pragma once

#include "x11/TouchEventSource.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPointF>
#include <QWidget>

class QScreen;
class QTimer;

namespace xen {

class TouchIndicatorOverlay : public QWidget {
    Q_OBJECT
public:
    explicit TouchIndicatorOverlay(QWidget* parent = nullptr);

    void showOnScreen(QScreen* screen);

public slots:
    void onTouch(int id, xen::TouchEventSource::Phase phase, double nx, double ny);

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    struct Ripple {
        QPointF pos;   // local pixels
        bool active = false;
        int ageMs = 0; // since release
    };
    QHash<int, Ripple> m_ripples;
    QElapsedTimer m_clock;
    QTimer* m_timer = nullptr;
};

} // namespace xen
