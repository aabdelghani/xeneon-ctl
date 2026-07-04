// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/CalibrationOverlay.h"

#include <cmath>

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTouchEvent>
#include <QWindow>

#include <array>

namespace xen {

namespace {
const QColor kAccent(0xEC, 0xE8, 0x1A);

// Solve a 3x3 linear system A x = b (Gaussian elimination). Returns false if
// singular.
bool solve3(double A[3][3], double b[3], double x[3])
{
    for (int col = 0; col < 3; ++col) {
        int piv = col;
        for (int r = col + 1; r < 3; ++r)
            if (std::abs(A[r][col]) > std::abs(A[piv][col]))
                piv = r;
        if (std::abs(A[piv][col]) < 1e-12)
            return false;
        if (piv != col) {
            for (int c = 0; c < 3; ++c)
                std::swap(A[piv][c], A[col][c]);
            std::swap(b[piv], b[col]);
        }
        for (int r = 0; r < 3; ++r) {
            if (r == col)
                continue;
            const double f = A[r][col] / A[col][col];
            for (int c = 0; c < 3; ++c)
                A[r][c] -= f * A[col][c];
            b[r] -= f * b[col];
        }
    }
    for (int i = 0; i < 3; ++i)
        x[i] = b[i] / A[i][i];
    return true;
}

// Least-squares affine: find (a,b,c) minimizing sum (a*px+b*py+c - q)^2.
bool fitAffineRow(const QVector<QPointF>& src, const QVector<double>& q, double out[3])
{
    double A[3][3] = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };
    double rhs[3] = { 0, 0, 0 };
    for (int i = 0; i < src.size(); ++i) {
        const double px = src[i].x();
        const double py = src[i].y();
        const double basis[3] = { px, py, 1.0 };
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c)
                A[r][c] += basis[r] * basis[c];
            rhs[r] += basis[r] * q[i];
        }
    }
    return solve3(A, rhs, out);
}
} // namespace

CalibrationOverlay::CalibrationOverlay(TouchControl* touch, QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_touch(touch)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setWindowTitle(QStringLiteral("Xeneon Edge — Touch Calibration"));
    m_status = tr("Tap each highlighted target with the tip of your finger.");
}

void CalibrationOverlay::showOnScreen(QScreen* screen)
{
    m_screen = screen ? screen : QGuiApplication::primaryScreen();

    // Baseline: start from the clean geometric mapping so taps land near the
    // targets and stay on this overlay while we measure.
    xen::TouchControl::applyOutputMapping();
    m_baseMatrix = xen::TouchControl::matrix();

    winId();
    if (windowHandle())
        windowHandle()->setScreen(m_screen);
    setGeometry(m_screen->geometry());

    const QSizeF s(width(), height());
    const std::array<QPointF, 5> fr = { QPointF(0.12, 0.18), QPointF(0.88, 0.18),
                                        QPointF(0.50, 0.50), QPointF(0.12, 0.82),
                                        QPointF(0.88, 0.82) };
    for (const QPointF& f : fr)
        m_targets.append(QPointF(f.x() * s.width(), f.y() * s.height()));

    showFullScreen();
    raise();
    activateWindow();
}

void CalibrationOverlay::recordHit(QPointF localPos)
{
    if (m_index >= m_targets.size())
        return;
    m_measured.append(localPos);
    ++m_index;
    if (m_index >= m_targets.size())
        solveAndApply();
    update();
}

bool CalibrationOverlay::event(QEvent* e)
{
    if (e->type() == QEvent::TouchBegin) {
        auto* te = static_cast<QTouchEvent*>(e);
        if (!te->points().isEmpty())
            recordHit(te->points().first().position());
        return true;
    }
    return QWidget::event(e);
}

void CalibrationOverlay::mousePressEvent(QMouseEvent* e)
{
    recordHit(e->position());
}

void CalibrationOverlay::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        emit finished(false);
        close();
    }
}

void CalibrationOverlay::solveAndApply()
{
    // Virtual desktop geometry: the matrix maps device-normalized coords into
    // the whole X screen, so normalize against the virtual bounds.
    const QRect vg = m_screen->virtualGeometry();
    const double W = vg.width();
    const double H = vg.height();
    const QPointF org = m_screen->geometry().topLeft() - vg.topLeft();

    // Invert the base (measurement) matrix affine to recover raw device coords.
    const double a = m_baseMatrix[0];
    const double b = m_baseMatrix[1];
    const double c = m_baseMatrix[2];
    const double d = m_baseMatrix[3];
    const double e = m_baseMatrix[4];
    const double f = m_baseMatrix[5];
    const double det = a * e - b * d;
    if (std::abs(det) < 1e-12) {
        m_status = tr("Calibration failed (degenerate base matrix).");
        emit finished(false);
        update();
        return;
    }
    auto invApply = [&](double sx, double sy, double& dx, double& dy) {
        dx = (e * (sx - c) - b * (sy - f)) / det;
        dy = (-d * (sx - c) + a * (sy - f)) / det;
    };

    QVector<QPointF> raw;         // device-normalized coords produced by taps
    QVector<double> tx;
    QVector<double> ty;       // desired screen-normalized target coords
    for (int i = 0; i < m_measured.size(); ++i) {
        const double pnx = (m_measured[i].x() + org.x()) / W;
        const double pny = (m_measured[i].y() + org.y()) / H;
        double dnx = NAN;
        double dny = NAN;
        invApply(pnx, pny, dnx, dny);
        raw.append(QPointF(dnx, dny));
        tx.append((m_targets[i].x() + org.x()) / W);
        ty.append((m_targets[i].y() + org.y()) / H);
    }

    double rowX[3];
    double rowY[3];
    if (!fitAffineRow(raw, tx, rowX) || !fitAffineRow(raw, ty, rowY)) {
        m_status = tr("Calibration failed (could not solve). Try again.");
        emit finished(false);
        update();
        return;
    }

    QList<double> const M{ rowX[0], rowX[1], rowX[2], rowY[0], rowY[1], rowY[2], 0, 0, 1 };
    const bool ok = xen::TouchControl::setMatrix(M);
    m_status = ok ? tr("Calibration applied. Tap anywhere to verify, Esc to finish.")
                  : tr("Could not write the matrix (xinput error).");
    emit finished(ok);
    update();
}

void CalibrationOverlay::paintEvent(QPaintEvent*)
{
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.fillRect(rect(), QColor(10, 11, 13, 235));

    QFont f = g.font();
    f.setPointSize(14);
    f.setBold(true);
    g.setFont(f);
    g.setPen(QColor(255, 255, 255, 200));
    g.drawText(rect().adjusted(0, 24, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
               tr("TOUCH CALIBRATION"));
    f.setPointSize(11);
    f.setBold(false);
    g.setFont(f);
    g.setPen(QColor(200, 200, 200, 200));
    g.drawText(rect().adjusted(0, 52, 0, 0), Qt::AlignHCenter | Qt::AlignTop, m_status);

    const bool done = m_index >= m_targets.size();

    // Already-tapped targets: dim check.
    for (int i = 0; i < m_measured.size(); ++i) {
        g.setPen(Qt::NoPen);
        g.setBrush(QColor(80, 200, 120, 180));
        g.drawEllipse(m_targets[i], 10, 10);
    }

    // Current (or all-done) target: bright pulsing crosshair.
    if (!done) {
        const QPointF t = m_targets[m_index];
        g.setPen(QPen(kAccent, 2));
        g.setBrush(Qt::NoBrush);
        g.drawEllipse(t, 26, 26);
        g.drawEllipse(t, 12, 12);
        g.drawLine(QPointF(t.x() - 40, t.y()), QPointF(t.x() + 40, t.y()));
        g.drawLine(QPointF(t.x(), t.y() - 40), QPointF(t.x(), t.y() + 40));
        g.setPen(QColor(255, 255, 255, 220));
        g.drawText(QPointF(t.x() + 34, t.y() - 30),
                   tr("target %1 / %2").arg(m_index + 1).arg(m_targets.size()));
    }
}

} // namespace xen
