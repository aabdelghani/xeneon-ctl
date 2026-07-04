// xeneon-ctl — Probe page: read-only recon (M3) + first gated write (M4).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPlainTextEdit>
#include <QWidget>

namespace xen {

class ProbePage : public QWidget {
    Q_OBJECT
public:
    explicit ProbePage(QWidget* parent = nullptr);

private:
    void runProbe();
    void queryFirmware(); // M4: confirm-gated write

    QPlainTextEdit* m_out = nullptr;
};

} // namespace xen
