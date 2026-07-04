// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/pages/ProbePage.h"

#include "core/WriteGate.h"
#include "proto/Commands.h"
#include "transport/HidEnumerator.h"
#include "transport/HidRecon.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace xen {

namespace {

QString hexBlock(const std::vector<uint8_t>& bytes, int perLine = 16)
{
    QString s;
    for (size_t i = 0; i < bytes.size(); ++i) {
        s += QString::asprintf("%02X ", bytes[i]);
        if ((i + 1) % perLine == 0)
            s += '\n';
    }
    return s.trimmed();
}

} // namespace

ProbePage::ProbePage(QWidget* parent)
    : QWidget(parent), m_out(new QPlainTextEdit(this))
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->setSpacing(12);

    auto* header = new QFrame(this);
    header->setObjectName(QStringLiteral("card"));
    auto* hl = new QVBoxLayout(header);
    hl->setContentsMargins(20, 14, 20, 14);
    auto* title = new QLabel(tr("PROTOCOL RECON — READ ONLY"), header);
    title->setObjectName(QStringLiteral("cardTitle"));
    hl->addWidget(title);
    auto* note = new QLabel(
        tr("Enumerates the device, dumps its report descriptor from sysfs, and does one "
           "passive read. No report is ever written to the device in this step."),
        header);
    note->setObjectName(QStringLiteral("cardSubtitle"));
    note->setWordWrap(true);
    hl->addWidget(note);
    auto* btnRow = new QHBoxLayout;
    auto* btn = new QPushButton(tr("Run probe (read-only)"), header);
    btn->setObjectName(QStringLiteral("actionButton"));
    connect(btn, &QPushButton::clicked, this, &ProbePage::runProbe);
    btnRow->addWidget(btn);

    auto* fw = new QPushButton(tr("Query firmware version…"), header);
    fw->setObjectName(QStringLiteral("actionButton"));
    connect(fw, &QPushButton::clicked, this, &ProbePage::queryFirmware);
    btnRow->addWidget(fw);
    btnRow->addStretch(1);
    hl->addLayout(btnRow);
    lay->addWidget(header);

    
    m_out->setObjectName(QStringLiteral("console"));
    m_out->setReadOnly(true);
    m_out->setLineWrapMode(QPlainTextEdit::NoWrap);
    lay->addWidget(m_out, 1);

    runProbe();
}

void ProbePage::runProbe()
{
    ReconReport const r = HidRecon::run();
    QString t;
    if (!r.haveInfo) {
        m_out->setPlainText(tr("No Xeneon Edge found."));
        return;
    }
    t += tr("XENEON EDGE  (read-only probe — no writes sent)\n");
    t += QStringLiteral("  product:    %1\n").arg(QString::fromStdString(r.info.product));
    t += QStringLiteral("  serial:     %1\n").arg(QString::fromStdString(r.info.serial));
    t += QStringLiteral("  hidraw:     %1\n").arg(QString::fromStdString(r.info.path));
    t += QStringLiteral("  usage page: 0x%1\n").arg(r.info.usagePage, 4, 16, QLatin1Char('0'));

    t += QStringLiteral("\nReport descriptor (%1 bytes, %2):\n")
             .arg(r.reportDescriptor.size())
             .arg(QString::fromStdString(r.descriptorSource));
    t += hexBlock(r.reportDescriptor) + "\n";
    t += "\nDecode:\n" + QString::fromStdString(r.descriptorDecode);

    t += "\nPassive read: " + QString::fromStdString(r.passiveNote) + "\n";
    if (!r.passiveReport.empty())
        t += "  bytes: " + hexBlock(r.passiveReport, 32) + "\n";

    m_out->setPlainText(t);
}

void ProbePage::queryFirmware()
{
    auto info = HidEnumerator::findEdge();
    if (!info || !info->accessible) {
        QMessageBox::warning(this, tr("Firmware query"),
                             tr("Edge not accessible over HID. Install the udev rule first."));
        return;
    }

    BragiFrame frame = buildFirmwareQuery();
    QString txHex;
    for (size_t i = 0; i < xen::BragiFrame::size(); ++i)
        txHex += QString::asprintf("%02X ", frame.data()[i]);

    QMessageBox box(this);
    box.setWindowTitle(tr("Confirm first write to the Edge"));
    box.setIcon(QMessageBox::Question);
    box.setText(tr("This sends the firmware-version query (a read request) to %1.\n\n"
                   "It is a GET command and does not change any device setting.")
                    .arg(info->path.c_str()));
    box.setInformativeText(tr("Exact 64 bytes to be written:\n%1\n\n"
                              "The transfer is logged to:\n%2")
                               .arg(txHex.trimmed(), WriteGate::logFilePath()));
    box.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
    box.setDefaultButton(QMessageBox::Cancel);
    box.button(QMessageBox::Ok)->setText(tr("Send"));
    if (box.exec() != QMessageBox::Ok)
        return;

    Confirmation const conf = Confirmation::approve(QStringLiteral("firmware-version query"));
    Exchange const ex = WriteGate::exchange(QString::fromStdString(info->path), frame, conf);

    QString r;
    r += tr("=== Firmware version query (M4) ===\n");
    r += tr("TX (64B): ");
    for (uint8_t const b : ex.tx)
        r += QString::asprintf("%02X ", b);
    r += QLatin1Char('\n');
    if (!ex.error.isEmpty()) {
        r += tr("ERROR: %1\n").arg(ex.error);
    } else if (ex.rx.empty()) {
        r += tr("RX: (no response within timeout)\n"
                "The command byte or framing may differ; see PROTOCOL.md candidates.\n");
    } else {
        r += tr("RX (%1B): ").arg(ex.rx.size());
        for (uint8_t const b : ex.rx)
            r += QString::asprintf("%02X ", b);
        r += QLatin1Char('\n');
        // Heuristic: many Bragi replies echo the command then carry an ASCII or
        // BCD version. Show any printable ASCII run as a hint.
        QString ascii;
        for (uint8_t const b : ex.rx)
            ascii += (b >= 0x20 && b < 0x7F) ? QChar(b) : QChar('.');
        r += tr("RX ascii: %1\n").arg(ascii);
    }
    r += tr("Logged to: %1\n").arg(ex.logPath);

    m_out->setPlainText(r);
}

} // namespace xen
