// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/DdcClient.h"

#include <QRegularExpression>

namespace xen {

DdcClient::DdcClient(QObject* parent)
    : QObject(parent)
{
    m_proc.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) { finishJob(code); });
}

void DdcClient::start()
{
    enqueue({ Job::Detect, 0, 0 });
}

void DdcClient::getVcp(quint8 code)
{
    enqueue({ Job::Get, code, 0 });
}

void DdcClient::setVcp(quint8 code, quint16 value)
{
    // Coalesce: replace a queued (not yet running) set for the same code.
    for (auto& j : m_queue) {
        if (j.kind == Job::Set && j.code == code) {
            j.value = value;
            return;
        }
    }
    enqueue({ Job::Set, code, value });
}

void DdcClient::enqueue(const Job& job)
{
    m_queue.append(job);
    if (!m_running)
        startNext();
}

void DdcClient::startNext()
{
    if (m_queue.isEmpty())
        return;
    m_current = m_queue.takeFirst();
    m_running = true;

    QStringList args;
    switch (m_current.kind) {
    case Job::Detect:
        args = { QStringLiteral("detect"), QStringLiteral("--brief") };
        break;
    case Job::Get:
        args = { QStringLiteral("--bus"), QString::number(m_bus),
                 QStringLiteral("--sleep-multiplier"), QStringLiteral(".4"),
                 QStringLiteral("getvcp"),
                 QString::number(m_current.code, 16) };
        break;
    case Job::Set:
        args = { QStringLiteral("--bus"), QString::number(m_bus),
                 QStringLiteral("--noverify"),
                 QStringLiteral("--sleep-multiplier"), QStringLiteral(".4"),
                 QStringLiteral("setvcp"),
                 QString::number(m_current.code, 16),
                 QString::number(m_current.value) };
        break;
    }
    m_proc.start(QStringLiteral("ddcutil"), args);
}

void DdcClient::finishJob(int exitCode)
{
    const QString out = QString::fromUtf8(m_proc.readAll());
    const Job job = m_current;
    m_running = false;

    switch (job.kind) {
    case Job::Detect: {
        // Find the display block naming the Edge and grab its /dev/i2c-N.
        int newBus = -1;
        const QStringList blocks = out.split(QStringLiteral("Display "));
        for (const QString& b : blocks) {
            if (b.contains(QStringLiteral("XENEON EDGE"))) {
                QRegularExpression re(QStringLiteral("/dev/i2c-(\\d+)"));
                const auto m = re.match(b);
                if (m.hasMatch())
                    newBus = m.captured(1).toInt();
            }
        }
        m_bus = newBus;
        emit readyChanged(m_bus >= 0,
                          m_bus >= 0
                              ? tr("Edge found on i2c bus %1").arg(m_bus)
                              : tr("Edge not found by ddcutil (is it connected via DisplayPort/HDMI?)"));
        break;
    }
    case Job::Get: {
        if (exitCode == 0) {
            // Continuous: "current value =    95, max value =   100"
            // Non-continuous (ddcutil 1.4): "...): User 1 (0x0b), Tolerance..."
            //                or older style: "... (sl=0x05)"
            QRegularExpression cont(
                QStringLiteral("current value\\s*=\\s*(\\d+),\\s*max value\\s*=\\s*(\\d+)"));
            QRegularExpression nc(
                QStringLiteral("(?:sl=0x|\\(0x)([0-9a-fA-F]+)\\)?"));
            if (auto m = cont.match(out); m.hasMatch()) {
                emit vcpRead(job.code, m.captured(1).toUShort(), m.captured(2).toUShort());
            } else if (auto n = nc.match(out); n.hasMatch()) {
                emit vcpRead(job.code, n.captured(1).toUShort(nullptr, 16), 0);
            } else {
                emit errorOccurred(tr("getvcp %1: unparsed output: %2")
                                       .arg(job.code, 0, 16).arg(out.trimmed()));
            }
        } else {
            emit errorOccurred(tr("getvcp 0x%1 failed: %2").arg(job.code, 0, 16).arg(out.trimmed()));
        }
        break;
    }
    case Job::Set: {
        if (exitCode == 0)
            emit vcpWritten(job.code, job.value);
        else
            emit errorOccurred(tr("setvcp 0x%1 failed: %2").arg(job.code, 0, 16).arg(out.trimmed()));
        break;
    }
    }
    startNext();
}

} // namespace xen
