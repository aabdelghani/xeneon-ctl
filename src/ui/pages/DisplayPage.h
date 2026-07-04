// xeneon-ctl — Display page: DDC/CI picture controls.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/DdcClient.h"

#include <QComboBox>
#include <QLabel>
#include <QMap>
#include <QSlider>
#include <QTimer>
#include <QWidget>

class QGridLayout;

namespace xen {

class DisplayPage : public QWidget {
    Q_OBJECT
public:
    explicit DisplayPage(DdcClient* ddc, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* ev) override;

private slots:
    void onReady(bool ready, const QString& message);
    void onVcpRead(quint8 code, quint16 current, quint16 max);
    void onError(const QString& message);

private:
    struct SliderRow {
        QSlider* slider = nullptr;
        QLabel* value = nullptr;
        QTimer* debounce = nullptr;
    };

    QWidget* buildPictureCard();
    QWidget* buildColorCard();
    QWidget* buildPowerCard();
    void addSliderRow(QGridLayout* grid, int row, const QString& label, quint8 code);
    void refresh();

    DdcClient* m_ddc;
    QMap<quint8, SliderRow> m_rows;
    QComboBox* m_preset = nullptr;
    QLabel* m_status = nullptr;
    bool m_refreshed = false;
};

} // namespace xen
