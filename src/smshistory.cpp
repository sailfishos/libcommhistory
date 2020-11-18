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

#include <QtDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>

namespace {

const QDateTime epoch(QDate(1970, 1, 1), QTime(0, 0), Qt::UTC);

QString buildEventsQuery(CommHistory::SMSHistory::TimeInterval timeInterval,
                         const QDateTime &startTime,
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

    switch (timeInterval) {
    case CommHistory::SMSHistory::NoTimeInterval:
        break;
    case CommHistory::SMSHistory::Yearly:
        group = groupTemplate.arg("%Y");
        break;
    case CommHistory::SMSHistory::Monthly:
        group = groupTemplate.arg("%Y-%m");
        break;
    case CommHistory::SMSHistory::Weekly:
        group = groupTemplate.arg("%Y-%W");
        break;
    case CommHistory::SMSHistory::Daily:
        group = groupTemplate.arg("%Y-%m-%d");
        break;
    }

    QString q = "SELECT startTime, remoteUid from Events";
    if (!conditions.isEmpty()) {
        q += " WHERE " + conditions.join(" AND ");
    }

    return q + group;
}

CommHistory::SMSHistory::Result readNextResult(QSqlQuery *query)
{
    CommHistory::SMSHistory::Result result;
    if (!query->next()) {
        return result;
    }
    result.when = QDateTime::fromMSecsSinceEpoch(query->value(0).toLongLong() * 1000).toUTC();
    result.phoneNumber = query->value(1).toString();
    return result;
}

bool dateMatchesForInterval(const QDate &d1, const QDate &d2, CommHistory::SMSHistory::TimeInterval timeInterval)
{
    switch (timeInterval) {
    case CommHistory::SMSHistory::NoTimeInterval:
        return true;
    case CommHistory::SMSHistory::Yearly:
        return d1.year() == d2.year();
    case CommHistory::SMSHistory::Monthly:
        return d1.year() == d2.year() && d1.month() == d2.month();
    case CommHistory::SMSHistory::Weekly:
    {
        int d1YearNumber = 0;
        int d1WeekNumber = d1.weekNumber(&d1YearNumber);
        int d2YearNumber = 0;
        int d2WeekNumber = d2.weekNumber(&d2YearNumber);
        return d1YearNumber == d2YearNumber && d1WeekNumber == d2WeekNumber;
    }
    case CommHistory::SMSHistory::Daily:
        return d1 == d2;
    }
    return false;
}

QList<CommHistory::SMSHistory::Result> readQueryResults(CommHistory::SMSHistory::TimeInterval timeInterval,
                                                        const QDateTime &startTime,
                                                        const QDateTime &endTime,
                                                        QSqlQuery *query)
{
    QList<CommHistory::SMSHistory::Result> results;

    if (timeInterval == CommHistory::SMSHistory::NoTimeInterval) {
        CommHistory::SMSHistory::Result result = readNextResult(query);
        if (result.when.isValid()) {
            results.append(result);
        }
        return results;
    }

    const QDateTime &startTimeUtc = startTime.toUTC();
    const QDateTime &endTimeUtc = endTime.toUTC();
    const QDate &startDate = startTimeUtc.date();
    const QDate &endDate = endTimeUtc.date();

    QDate nextDate = startDate;
    CommHistory::SMSHistory::Result rowResult;
    bool readNewResult = true;

    while (nextDate <= endDate) {
        if (readNewResult) {
            rowResult = readNextResult(query);
        }

        const QDateTime &rowDateTime = rowResult.when;
        const QDate &rowDate = rowDateTime.date();

        CommHistory::SMSHistory::Result newResult;
        if (dateMatchesForInterval(nextDate, startDate, timeInterval)) {
            newResult.when = startTimeUtc;
        } else {
            newResult.when = QDateTime(nextDate, QTime(), Qt::UTC);
        }

        if (rowResult.when.isValid() && dateMatchesForInterval(nextDate, rowDate, timeInterval)) {
            newResult.phoneNumber = rowResult.phoneNumber;
            readNewResult = true;
        } else {
            readNewResult = false;
        }

        if (readNewResult) {
            results.append(newResult);
        }

        switch (timeInterval) {
        case CommHistory::SMSHistory::NoTimeInterval:
            break;
        case CommHistory::SMSHistory::Yearly:
            nextDate.setDate(nextDate.year() + 1, 1, 1);
            break;
        case CommHistory::SMSHistory::Monthly:
            nextDate = nextDate.addMonths(1);
            nextDate.setDate(nextDate.year(), nextDate.month(), 1);
            break;
        case CommHistory::SMSHistory::Weekly:
            nextDate = nextDate.addDays(Qt::Sunday - nextDate.dayOfWeek() + 1);
            break;
        case CommHistory::SMSHistory::Daily:
            nextDate = nextDate.addDays(1);
            break;
        }
    }

    return results;
}

}

namespace CommHistory {

SMSHistoryPrivate::SMSHistoryPrivate(SMSHistory *parent)
    : QObject(parent)
    , q(parent)
    , timeInterval(SMSHistory::NoTimeInterval)
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

void SMSHistory::setTimeInterval(TimeInterval timeInterval)
{
    if (timeInterval != d->timeInterval) {
        d->timeInterval = timeInterval;
        emit timeIntervalChanged();
    }
}

SMSHistory::TimeInterval SMSHistory::timeInterval() const
{
    return d->timeInterval;
}

QList<CommHistory::SMSHistory::Result> SMSHistory::results() const
{
    return d->results;
}

bool SMSHistory::reload()
{
    d->results.clear();

    if (d->startTime.isValid() && d->endTime.isValid() && d->endTime <= d->startTime) {
        qWarning() << "Error: end time" << d->endTime.toString()
                   << "is not after start time" << d->startTime.toString();
        return false;
    }

    QString queryString = buildEventsQuery(d->timeInterval, d->startTime, d->endTime);

    QSqlQuery query = DatabaseIOPrivate::prepareQuery(queryString);
    if (!query.exec()) {
        qWarning() << "Failed to execute query:" << query.lastQuery();
        qWarning() << "Error was:" << query.lastError();
        return false;
    }

    d->results = readQueryResults(d->timeInterval, d->startTime, d->endTime, &query);
    return true;
}

}
