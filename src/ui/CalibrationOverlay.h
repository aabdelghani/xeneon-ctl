// xeneon-ctl — touch calibration on the Xeneon Edge (M5.2).
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Shows a sequence of targets on the Edge; you tap each one. From where the
// pointer actually lands (with the current matrix applied) versus where the
// target is, it solves an affine correction and writes a new Coordinate
// Transformation Matrix so future touches land under your finger.
#pragma once

#include "core/TouchControl.h"

#include <QList>
#include <QPointF>
#include <QVector>
#include <QWidget>

class QScreen;

namespace xen {

class CalibrationOverlay : public QWidget {
    Q_OBJECT
public:
    explicit CalibrationOverlay(TouchControl* touch, QWidget* parent = nullptr);
    void showOnScreen(QScreen* screen);

signals:
    void finished(bool applied);

protected:
    bool event(QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;

private:
    void recordHit(QPointF localPos);
    void solveAndApply();

    TouchControl* m_touch;
    QScreen* m_screen = nullptr;
    QVector<QPointF> m_targets;  // Edge-local pixel targets
    QVector<QPointF> m_measured; // Edge-local pixel where pointer landed
    QList<double> m_baseMatrix;  // matrix in effect during measurement
    int m_index = 0;
    QString m_status;
};

} // namespace xen
