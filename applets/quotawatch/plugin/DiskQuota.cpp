/*
 * Copyright (C) 2015 Dominik Haumann <dhaumann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "DiskQuota.h"
#include "QuotaItem.h"
#include "QuotaListModel.h"

#include <KLocalizedString>
#include <KFormat>

#include <QTimer>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDebug>

DiskQuota::DiskQuota(QObject * parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_quotaInstalled(true)
    , m_status(QStringLiteral("quota-ok"))
    , m_model(new QuotaListModel(this))
{
    connect(m_timer, &QTimer::timeout, this, &DiskQuota::updateQuota);
    m_timer->start(5 * 1000);
    updateQuota();
}

bool DiskQuota::quotaInstalled() const
{
    return m_quotaInstalled;
}

void DiskQuota::setQuotaInstalled(bool installed)
{
    if (m_quotaInstalled != installed) {
        m_quotaInstalled = installed;

        if (! installed) {
            m_model->clear();
            setStatus(QStringLiteral("status-not-installed"));
            setToolTip(i18n("Disk Quota"));
            setSubToolTip(i18n("Please install 'quota'"));
        }

        emit quotaInstalledChanged();
    }
}

QString DiskQuota::status() const
{
    return m_status;
}

void DiskQuota::setStatus(const QString & status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

QString DiskQuota::toolTip() const
{
    return m_toolTip;
}

void DiskQuota::setToolTip(const QString & toolTip)
{
    if (m_toolTip != toolTip) {
        m_toolTip = toolTip;
        emit toolTipChanged();
    }
}

QString DiskQuota::subToolTip() const
{
    return m_subToolTip;
}

void DiskQuota::setSubToolTip(const QString & subToolTip)
{
    if (m_subToolTip != subToolTip) {
        m_subToolTip = subToolTip;
        emit subToolTipChanged();
    }
}

static bool isQuotaLine(const QString & line)
{
    const int iMax = line.size();
    for (int i = 0; i < iMax; ++i) {
        if (!line[i].isSpace() && line[i] == QLatin1Char('/')) {
            return true;
        }
    }
    return false;
}

static bool runQuotaApp(QString & stdout)
{
    // Try to run 'quota'
    const QStringList args = QStringList()
        << QStringLiteral("--show-mntpoint")      // second entry is e.g. '/home'
        << QStringLiteral("--hide-device")        // hide e.g. /dev/sda3
        << QStringLiteral("--no-mixed-pathnames") // trim leading slashes from NFSv4 mountpoints
        << QStringLiteral("--all-nfs")            // show all mount points
        << QStringLiteral("--no-wrap")            // do not wrap long lines
        << QStringLiteral("--quiet-refuse");      // no not print error message when NFS server does not respond

    QProcess proc;
    proc.start(QStringLiteral("quota"), args);

    const bool success = proc.waitForStarted() && proc.waitForFinished();
    if (!success) {
        return false;
    }

    // get stdout
    proc.closeWriteChannel();
    stdout = QString::fromLocal8Bit(proc.readAllStandardOutput());

    return true;
}

void DiskQuota::updateQuota()
{
    const bool quotaFound = ! QStandardPaths::findExecutable(QStringLiteral("quota")).isEmpty();
    setQuotaInstalled(quotaFound);
    if (!quotaFound) {
        return;
    }

    // get quota output
    QString rawData;
    const bool success = runQuotaApp(rawData);
    if (!success) {
        m_model->clear();
        setToolTip(i18n("Disk Quota"));
        setSubToolTip(i18n("Running quota failed"));
        return;
    }

//     const QStringList lines = rawData.split(QRegularExpression(QStringLiteral("[\r\n]")), QString::SkipEmptyParts);
    QStringList lines = QStringList()
//         << QStringLiteral("/home/peterpan 4471196*  5000000 7000000           57602 0       0")
//         << QStringLiteral("/home/archive 2263536  5000000 5100000            3932 0       0")
//         << QStringLiteral("/home/shared 3171196*  5000000 7000000           57602 0       0");
        << QStringLiteral("/home/peterpan %1*  5000000 7000000           57602 0       0").arg(qrand() % 5000000)
        << QStringLiteral("/home/archive %1  5000000 5100000            3932 0       0").arg(qrand() % 5000000)
        << QStringLiteral("/home/shared %1*  5000000 7000000           57602 0       0").arg(qrand() % 5000000);
//     lines.removeAt(qrand() % lines.size());

    // format class needed for GiB/MiB/KiB formatting
    KFormat fmt;
    int maxQuota = 0;
    QVector<QuotaItem> items;

    // assumption: Filesystem starts with slash
    for (const QString & line : lines) {
        qDebug() << line << isQuotaLine(line);
        if (!isQuotaLine(line)) {
            continue;
        }

        QStringList parts = line.split(QLatin1Char(' '), QString::SkipEmptyParts);
        // valid lines range from 7 to 9 parts (grace not always there):
        // Disk quotas for user dh (uid 1000):
        //      Filesystem   blocks  quota    limit     grace   files    quota   limit   grace
        //       /home     16296500  50000000 60000000          389155       0       0
        //       /home     16296500* 50000000 60000000      6   389155       0       0
        //       /home     16296500* 50000000 60000000      4   389155       0       0       5
        //       ^...........we want these...........^
        // NOTE: In case of a soft limit violation, a '*' is added in the used blocks.
        //       Hence, the star is removed below, if applicable

        if (parts.size() < 4) {
            continue;
        }

        // 'quota' uses kilo bytes -> factor 1024
        // NOTE: int is not large enough, hence qint64
        const qint64 used = parts[1].remove(QLatin1Char('*')).toLongLong() * 1024;
        qint64 softLimit = parts[2].toLongLong() * 1024;
        const qint64 hardLimit = parts[3].toLongLong() * 1024;
        if (softLimit == 0) { // softLimit might be unused (0)
            softLimit = hardLimit;
        }
        const qint64 freeSize = softLimit - used;
        const int percent = qMin(100, qMax(0, qRound(used * 100.0 / softLimit)));

        QuotaItem item;
        item.setIconName(QStringLiteral("network-server-database"));
        item.setMountPoint(parts[0]);
        item.setUsage(percent);
        item.setMountString(i18nc("usage of quota, e.g.: '/home/bla: 38\% used'", "%1: %2% used", parts[0], percent));
        item.setUsedString(i18nc("e.g.: 12 GiB of 20 GiB used", "%1 of %2 used", fmt.formatByteSize(used), fmt.formatByteSize(softLimit)));
        item.setFreeString(i18nc("e.g.: 8 GiB free", "%1 free", fmt.formatByteSize(qMax(qint64(0), freeSize))));

        items.append(item);

        maxQuota = qMax(0, percent);
    }

    m_model->updateItems(items);

//     qDebug() << "QUOTAS:" << quotas;

    maxQuota = qMin(100, maxQuota);

    // update status
    setStatus(maxQuota < 50 ? QStringLiteral("status-ok")
            : maxQuota < 75 ? QStringLiteral("status-75")
            : maxQuota < 90 ? QStringLiteral("status-90")
            : QStringLiteral("status-critical"));

    if (!items.isEmpty()) {
        setToolTip(i18nc("example: Quota: 83% used",
                         "Quota: %1% used", static_cast<int>(maxQuota)));
        setSubToolTip(QString());
    } else {
        setToolTip(i18n("Disk Quota"));
        setSubToolTip(i18n("No quota restrictions found."));
    }
}

QuotaListModel * DiskQuota::model() const
{
    return m_model;
}
