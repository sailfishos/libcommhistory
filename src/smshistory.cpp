/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (c) 2020 Open Mobile Platform LLC.
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

#include "smshistory_p.h"
#include "databaseio_p.h"
#include "event.h"
#include "debug_p.h"

#include <QtDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>

namespace {

const QDateTime epoch(QDate(1970, 1, 1), QTime(0, 0), Qt::UTC);

QString buildEventsQuery(const QDateTime &startTime,
                         const QDateTime &endTime)
{
    qint64 startTimeSecs = (startTime.isValid() ? startTime : epoch).toMSecsSinceEpoch() / 1000;
    qint64 endTimeSecs = (endTime.isValid() ? endTime : QDateTime::currentDateTimeUtc()).toMSecsSinceEpoch() / 1000;

    QStringList conditions;

    conditions.append(QString::fromLatin1("startTime >= %1").arg(startTimeSecs));
    conditions.append(QString::fromLatin1("startTime <= %1").arg(endTimeSecs));
    conditions.append(QString::fromLatin1("type=%1").arg(CommHistory::Event::EventType::SMSEvent));

    QString group;
    static const QString groupTemplate = QStringLiteral(" GROUP BY strftime('%1', datetime(startTime, 'unixepoch'))");

    QString q = "SELECT startTime, remoteUid from Events";
    if (!conditions.isEmpty()) {
        q += " WHERE " + conditions.join(" AND ");
    }

    return q + group;
}

QList<CommHistory::SMSHistory::Result> readQueryResult(QSqlQuery *query)
{
    QList<CommHistory::SMSHistory::Result> results;
    while (query->next()) {
        CommHistory::SMSHistory::Result result;
        result.when = QDateTime::fromMSecsSinceEpoch(query->value(0).toLongLong() * 1000).toUTC();
        result.phoneNumber = query->value(1).toString();
        results.append(result);
    }
    return results;
}

}

namespace CommHistory {

SMSHistoryPrivate::SMSHistoryPrivate(SMSHistory *parent)
    : QObject(parent)
    , q(parent)
{
}

SMSHistory::SMSHistory(QObject *parent)
    : QObject(parent)
    , d(new SMSHistoryPrivate(this))
{
}

void SMSHistory::setStartTime(const QDateTime &dt)
{
    if (dt != d->startTime) {
        d->startTime = dt;
        emit startTimeChanged();
    }
}

QDateTime SMSHistory::startTime() const
{
    return d->startTime;
}

void SMSHistory::setEndTime(const QDateTime &dt)
{
    if (dt != d->endTime) {
        d->endTime = dt;
        emit endTimeChanged();
    }
}

QDateTime SMSHistory::endTime() const
{
    return d->endTime;
}

QList<CommHistory::SMSHistory::Result> SMSHistory::results() const
{
    return d->results;
}

bool SMSHistory::reload()
{
    d->results.clear();

    if (d->startTime.isValid() && d->endTime.isValid() && d->endTime <= d->startTime) {
        qCWarning(lcCommHistory) << "Error: end time" << d->endTime.toString()
                                 << "is not after start time" << d->startTime.toString();
        return false;
    }

    QString queryString = buildEventsQuery(d->startTime, d->endTime);

    QSqlQuery query = DatabaseIOPrivate::prepareQuery(queryString);
    if (!query.exec()) {
        qCWarning(lcCommHistory) << "Failed to execute query:" << query.lastQuery();
        qCWarning(lcCommHistory) << "Error was:" << query.lastError();
        return false;
    }

    d->results = readQueryResult(&query);
    return true;
}

}
