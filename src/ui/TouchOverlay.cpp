// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/TouchOverlay.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QTouchEvent>
#include <QWindow>

namespace xen {

namespace {
constexpr int kMouseId = -1;
const QColor kAccent(0xEC, 0xE8, 0x1A);
} // namespace

TouchOverlay::TouchOverlay(QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    setCursor(Qt::BlankCursor);
    setWindowTitle(QStringLiteral("Xeneon Edge — Touch Indicator"));

    m_clock.start();
    // ~60 fps repaint so rings animate and released points fade out.
    auto* t = new QTimer(this);
    connect(t, &QTimer::timeout, this, [this] { update(); });
    t->start(16);
}

void TouchOverlay::showOnScreen(QScreen* screen)
{
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    // Attach the native window to the target screen before going fullscreen so
    // the compositor puts it on the Edge, not the primary monitor.
    winId(); // force native window/handle creation
    if (windowHandle())
        windowHandle()->setScreen(screen);
    setGeometry(screen->geometry());
    showFullScreen();
    raise();
    activateWindow();
}

void TouchOverlay::bump(int id, QPointF p, bool active)
{
    Touch& t = m_touches[id];
    t.pos = p;
    t.active = active;
    t.ageMs = 0;
}

bool TouchOverlay::event(QEvent* e)
{
    switch (e->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd: {
        auto* te = static_cast<QTouchEvent*>(e);
        for (const QTouchEvent::TouchPoint& p : te->points()) {
            const bool active = p.state() != QEventPoint::Released;
            bump(p.id(), p.position(), active);
        }
        update();
        return true;
    }
    default:
        break;
    }
    return QWidget::event(e);
}

void TouchOverlay::mousePressEvent(QMouseEvent* e)
{
    bump(kMouseId, e->position(), true);
    update();
}

void TouchOverlay::mouseMoveEvent(QMouseEvent* e)
{
    // Only track while pressed (touch drag); hover with no touch does nothing.
    if (e->buttons() != Qt::NoButton)
        bump(kMouseId, e->position(), true);
    update();
}

void TouchOverlay::mouseReleaseEvent(QMouseEvent* e)
{
    bump(kMouseId, e->position(), false);
    update();
}

void TouchOverlay::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(e);
}

void TouchOverlay::paintEvent(QPaintEvent*)
{
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);

    // Dim backdrop + guides so the touch surface is obvious.
    g.fillRect(rect(), QColor(10, 11, 13, 205));
    g.setPen(QPen(QColor(255, 255, 255, 22), 1));
    for (int x = 0; x <= width(); x += width() / 8)
        g.drawLine(x, 0, x, height());
    for (int y = 0; y <= height(); y += height() / 4)
        g.drawLine(0, y, width(), y);

    // Header hint.
    g.setPen(QColor(255, 255, 255, 140));
    QFont f = g.font();
    f.setPointSize(13);
    f.setBold(true);
    g.setFont(f);
    g.drawText(rect().adjusted(0, 18, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
               tr("TOUCH INDICATOR  ·  touch the panel  ·  press Esc to exit"));

    const int frame = static_cast<int>(m_clock.elapsed() / 16);

    // Advance ages and drop long-released points.
    QList<int> dead;
    for (auto it = m_touches.begin(); it != m_touches.end(); ++it) {
        if (!it->active) {
            it->ageMs += 16;
            if (it->ageMs > 500)
                dead.append(it.key());
        }
    }
    for (int const k : dead)
        m_touches.remove(k);

    for (auto it = m_touches.constBegin(); it != m_touches.constEnd(); ++it) {
        const Touch& t = it.value();
        const QPointF c = t.pos;
        const qreal fade = t.active ? 1.0 : qMax(0.0, 1.0 - t.ageMs / 500.0);

        // Crosshair spanning the screen at the touch point.
        g.setPen(QPen(QColor(kAccent.red(), kAccent.green(), kAccent.blue(),
                             int(60 * fade)), 1, Qt::DashLine));
        g.drawLine(QPointF(c.x(), 0), QPointF(c.x(), height()));
        g.drawLine(QPointF(0, c.y()), QPointF(width(), c.y()));

        // Animated expanding ring (only while active).
        if (t.active) {
            const qreal phase = (frame % 45) / 45.0;
            const qreal rr = 22 + phase * 34;
            g.setPen(QPen(QColor(kAccent.red(), kAccent.green(), kAccent.blue(),
                                 int(180 * (1.0 - phase))), 3));
            g.setBrush(Qt::NoBrush);
            g.drawEllipse(c, rr, rr);
        }

        // Solid dot.
        g.setPen(Qt::NoPen);
        g.setBrush(QColor(kAccent.red(), kAccent.green(), kAccent.blue(), int(230 * fade)));
        g.drawEllipse(c, 14, 14);
        g.setBrush(QColor(20, 20, 20, int(230 * fade)));
        g.drawEllipse(c, 5, 5);

        // Coordinate label.
        g.setPen(QColor(255, 255, 255, int(230 * fade)));
        f.setPointSize(11);
        f.setBold(false);
        g.setFont(f);
        const QString label =
            (it.key() == kMouseId ? QString() : tr("#%1  ").arg(it.key()))
            + QStringLiteral("%1, %2").arg(int(c.x())).arg(int(c.y()));
        g.drawText(QPointF(c.x() + 20, c.y() - 20), label);
    }
    ++m_frames;
}

} // namespace xen
