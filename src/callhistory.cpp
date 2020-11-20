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

#include "callhistory_p.h"
#include "databaseio_p.h"
#include "event.h"

#include <QtDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>

namespace {

const QDateTime epoch(QDate(1970, 1, 1), QTime(0, 0), Qt::UTC);

QString buildEventsQuery(CommHistory::CallEvent::CallType callType,
                         const QDateTime &startTime,
                         const QDateTime &endTime)
{
    qint64 startTimeSecs = (startTime.isValid() ? startTime : QDateTime::fromMSecsSinceEpoch(0)).toMSecsSinceEpoch() / 1000;
    qint64 endTimeSecs = (endTime.isValid() ? endTime : QDateTime::currentDateTimeUtc()).toMSecsSinceEpoch() / 1000;

    QStringList conditions;

    conditions.append(QStringLiteral("startTime >= %1").arg(startTimeSecs));
    conditions.append(QStringLiteral("startTime <= %1").arg(endTimeSecs));
    conditions.append(QStringLiteral("type=%1").arg(CommHistory::Event::EventType::CallEvent));

    switch (callType) {
    case CommHistory::CallEvent::UnknownCallType:
        break;
    case CommHistory::CallEvent::ReceivedCallType:
        conditions.append(QString::fromLatin1("direction=%1 AND isMissedCall=0").arg(CommHistory::Event::Inbound));
        break;
    case CommHistory::CallEvent::MissedCallType:
        conditions.append(QString::fromLatin1("direction=%1 AND isMissedCall=1").arg(CommHistory::Event::Inbound));
        break;
    case CommHistory::CallEvent::DialedCallType:
        conditions.append(QString::fromLatin1("direction=%1").arg(CommHistory::Event::Outbound));
        break;
    }

    QString group;
    static const QString groupTemplate = QStringLiteral(" GROUP BY strftime('%1', datetime(startTime, 'unixepoch'))");

    QString q = "SELECT startTime, endTime, remoteUid from Events";
    if (!conditions.isEmpty()) {
        q += " WHERE " + conditions.join(" AND ");
    }

    return q + group;
}

QList<CommHistory::CallHistory::Result> readQueryResult(QSqlQuery *query)
{
    QList<CommHistory::CallHistory::Result> results;
    while (query->next()) {
        CommHistory::CallHistory::Result result;
        result.when = QDateTime::fromMSecsSinceEpoch(query->value(0).toLongLong() * 1000).toUTC();
        result.finish = QDateTime::fromMSecsSinceEpoch(query->value(1).toLongLong() * 1000).toUTC();
        result.phoneNumber = query->value(2).toString();
        results.append(result);
    }
    return results;
}

}

namespace CommHistory {

CallHistoryPrivate::CallHistoryPrivate(CallHistory *parent)
    : QObject(parent)
    , q(parent)
    , callType(CallEvent::UnknownCallType)
{
}

CallHistory::CallHistory(QObject *parent)
    : QObject(parent)
    , d(new CallHistoryPrivate(this))
{
}

void CallHistory::setStartTime(const QDateTime &dt)
{
    if (dt != d->startTime) {
        d->startTime = dt;
        emit startTimeChanged();
    }
}

QDateTime CallHistory::startTime() const
{
    return d->startTime;
}

void CallHistory::setEndTime(const QDateTime &dt)
{
    if (dt != d->endTime) {
        d->endTime = dt;
        emit endTimeChanged();
    }
}

QDateTime CallHistory::endTime() const
{
    return d->endTime;
}

void CallHistory::setCallType(CallEvent::CallType type)
{
    if (type != d->callType) {
        d->callType = type;
        emit callTypeChanged();
    }
}

CallEvent::CallType CallHistory::callType() const
{
    return d->callType;
}

QList<CommHistory::CallHistory::Result> CallHistory::results() const
{
    return d->results;
}

bool CallHistory::reload()
{
    d->results.clear();

    if (d->startTime.isValid() && d->endTime.isValid() && d->endTime <= d->startTime) {
        qWarning() << "Error: end time" << d->endTime.toString()
                   << "is not after start time" << d->startTime.toString();
        return false;
    }

    QString queryString = buildEventsQuery(d->callType, d->startTime, d->endTime);

    QSqlQuery query = DatabaseIOPrivate::prepareQuery(queryString);
    if (!query.exec()) {
        qWarning() << "Failed to execute query:" << query.lastQuery();
        qWarning() << "Error was:" << query.lastError();
        return false;
    }

    d->results = readQueryResult(&query);
    return true;
}

}
