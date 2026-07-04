// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dashboard/DashboardWindow.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QWindow>

namespace xen {

namespace {
const QColor kBg(14, 15, 17);
const QColor kCard(28, 29, 33);
const QColor kCardEdge(42, 43, 48);
const QColor kText(230, 230, 233);
const QColor kMuted(139, 140, 146);
const QColor kAccentYellow(0xEC, 0xE8, 0x1A);
const QColor kAccentCyan(0x4A, 0xD8, 0xE0);
const QColor kAccentGreen(0x4A, 0xDE, 0x80);
const QColor kAccentViolet(0xB4, 0x8E, 0xF0);
} // namespace

DashboardWindow::DashboardWindow(QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setWindowTitle(QStringLiteral("Xeneon Edge — Dashboard"));
    setCursor(Qt::BlankCursor);

    m_sensors = new SensorSource(this);
    connect(m_sensors, &SensorSource::updated, this, [this](const SensorSnapshot& s) {
        m_snap = s;
        update();
    });

    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, [this] {
        const QDateTime now = QDateTime::currentDateTime();
        m_time = now.toString(QStringLiteral("HH:mm"));
        m_date = now.toString(QStringLiteral("dddd, d MMMM"));
        update();
    });
}

void DashboardWindow::showOnScreen(QScreen* screen)
{
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    winId();
    if (windowHandle())
        windowHandle()->setScreen(screen);
    setGeometry(screen->geometry());
    showFullScreen();
    raise();
    activateWindow();
    setFocus(); // so Esc reaches keyPressEvent

    const QDateTime now = QDateTime::currentDateTime();
    m_time = now.toString(QStringLiteral("HH:mm"));
    m_date = now.toString(QStringLiteral("dddd, d MMMM"));
    m_sensors->start(1000);
    m_clockTimer->start(1000);
}

void DashboardWindow::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        m_sensors->stop();
        close();
    }
}

void DashboardWindow::drawTile(QPainter& g, const QRectF& r, const QString& title,
                               const QString& big, const QString& sub, double pct,
                               const QColor& accent)
{
    QPainterPath card;
    card.addRoundedRect(r, 18, 18);
    g.fillPath(card, kCard);
    g.setPen(QPen(kCardEdge, 1));
    g.drawPath(card);

    const qreal pad = 22;
    QFont f = g.font();

    // Title
    f.setPointSizeF(13);
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    g.setFont(f);
    g.setPen(kMuted);
    g.drawText(QRectF(r.left() + pad, r.top() + pad, r.width() - 2 * pad, 24),
               Qt::AlignLeft | Qt::AlignVCenter, title);

    // Big value
    f.setPointSizeF(58);
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0);
    g.setFont(f);
    g.setPen(kText);
    g.drawText(QRectF(r.left() + pad, r.top() + pad + 26, r.width() - 2 * pad, 92),
               Qt::AlignLeft | Qt::AlignVCenter, big);

    // Sub value
    f.setPointSizeF(15);
    f.setBold(false);
    g.setFont(f);
    g.setPen(kMuted);
    g.drawText(QRectF(r.left() + pad, r.top() + pad + 122, r.width() - 2 * pad, 26),
               Qt::AlignLeft | Qt::AlignVCenter, sub);

    // Progress bar
    if (pct >= 0) {
        const QRectF track(r.left() + pad, r.bottom() - pad - 10, r.width() - 2 * pad, 8);
        QPainterPath tp;
        tp.addRoundedRect(track, 4, 4);
        g.fillPath(tp, QColor(46, 47, 52));
        QRectF fill = track;
        fill.setWidth(track.width() * qBound(0.0, pct / 100.0, 1.0));
        QPainterPath fp;
        fp.addRoundedRect(fill, 4, 4);
        g.fillPath(fp, accent);
    }
}

void DashboardWindow::paintEvent(QPaintEvent*)
{
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.fillRect(rect(), kBg);
    // subtle grid
    g.setPen(QPen(QColor(255, 255, 255, 8), 1));
    for (int x = 0; x < width(); x += 160)
        g.drawLine(x, 0, x, height());

    const qreal m = 26;      // outer margin
    const qreal gap = 20;    // between tiles
    const qreal top = m;
    const qreal h = height() - 2 * m;

    // Clock panel (left, wider), then CPU, GPU, RAM tiles.
    const qreal clockW = 640;
    const qreal tileW = (width() - 2 * m - clockW - 3 * gap) / 3.0;

    // --- Clock ---
    QRectF clockR(m, top, clockW, h);
    QPainterPath cp;
    cp.addRoundedRect(clockR, 18, 18);
    g.fillPath(cp, kCard);
    g.setPen(QPen(kCardEdge, 1));
    g.drawPath(cp);

    QFont f = g.font();
    f.setPointSizeF(150);
    f.setBold(true);
    g.setFont(f);
    g.setPen(kText);
    g.drawText(clockR.adjusted(30, -30, -30, -40), Qt::AlignCenter, m_time);
    f.setPointSizeF(22);
    f.setBold(false);
    g.setFont(f);
    g.setPen(kMuted);
    g.drawText(clockR.adjusted(30, 0, -30, -44), Qt::AlignHCenter | Qt::AlignBottom, m_date);
    // brand
    f.setPointSizeF(13);
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    g.setFont(f);
    g.setPen(kAccentYellow);
    g.drawText(clockR.adjusted(30, 24, -30, 0), Qt::AlignHCenter | Qt::AlignTop,
               QStringLiteral("XENEON EDGE"));

    // --- CPU ---
    auto fmt = [](double v, const char* suffix, int dec = 0) {
        return v < 0 ? QStringLiteral("--") : QString::number(v, 'f', dec) + QLatin1String(suffix);
    };
    QRectF cpuR(m + clockW + gap, top, tileW, h);
    drawTile(g, cpuR, QStringLiteral("CPU"), fmt(m_snap.cpuLoadPct, "%"),
             m_snap.cpuTempC < 0 ? QStringLiteral("temp --")
                                 : QStringLiteral("%1 °C").arg(m_snap.cpuTempC, 0, 'f', 0),
             m_snap.cpuLoadPct, kAccentYellow);

    // --- GPU ---
    QRectF gpuR(cpuR.right() + gap, top, tileW, h);
    drawTile(g, gpuR, QStringLiteral("GPU"), m_snap.gpuOk ? fmt(m_snap.gpuUtilPct, "%")
                                                          : QStringLiteral("--"),
             m_snap.gpuOk
                 ? QStringLiteral("%1 °C   %2/%3 GB")
                       .arg(m_snap.gpuTempC, 0, 'f', 0)
                       .arg(m_snap.gpuMemUsedGiB, 0, 'f', 1)
                       .arg(m_snap.gpuMemTotalGiB, 0, 'f', 0)
                 : QStringLiteral("nvidia-smi unavailable"),
             m_snap.gpuOk ? m_snap.gpuUtilPct : -1, kAccentCyan);

    // --- RAM ---
    QRectF ramR(gpuR.right() + gap, top, tileW, h);
    drawTile(g, ramR, QStringLiteral("MEMORY"), fmt(m_snap.ramPct, "%"),
             QStringLiteral("%1 / %2 GB").arg(m_snap.ramUsedGiB, 0, 'f', 1)
                 .arg(m_snap.ramTotalGiB, 0, 'f', 0),
             m_snap.ramPct, kAccentGreen);

    // hint
    f.setPointSizeF(11);
    f.setBold(false);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0);
    g.setFont(f);
    g.setPen(QColor(255, 255, 255, 60));
    g.drawText(rect().adjusted(0, 0, -14, -8), Qt::AlignRight | Qt::AlignBottom,
               QStringLiteral("Press Esc, or use the app button, to close"));
    Q_UNUSED(kAccentViolet);
}

} // namespace xen
