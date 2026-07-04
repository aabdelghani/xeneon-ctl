// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/MainWindow.h"

#include <QApplication>
#include <QFile>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("xeneon-ctl"));
    app.setOrganizationName(QStringLiteral("xeneon-ctl"));

    QFile qss(QStringLiteral(":/theme.qss"));
    if (qss.open(QIODevice::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    xen::MainWindow w;
    w.show();
    return app.exec();
}
