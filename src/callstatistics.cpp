#include "callstatistics_p.h"
#include "databaseio_p.h"
#include "event.h"

#include <QtDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>

namespace {

const QDateTime epoch(QDate(1970, 1, 1), QTime(0, 0), Qt::UTC);

QString buildEventsQuery(CommHistory::CallEvent::CallType callType,
                         CommHistory::CallStatistics::TimeInterval timeInterval,
                         const QDateTime &startTime,
                         const QDateTime &endTime)
{
    qint64 startTimeSecs = (startTime.isValid() ? startTime : epoch).toMSecsSinceEpoch() / 1000;
    qint64 endTimeSecs = (endTime.isValid() ? endTime : QDateTime::currentDateTimeUtc()).toMSecsSinceEpoch() / 1000;

    QStringList conditions;

    conditions.append(QString::fromLatin1("startTime >= %1").arg(startTimeSecs));
    conditions.append(QString::fromLatin1("startTime <= %1").arg(endTimeSecs));

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

    switch (timeInterval) {
    case CommHistory::CallStatistics::NoTimeInterval:
        break;
    case CommHistory::CallStatistics::Yearly:
        group = groupTemplate.arg("%Y");
        break;
    case CommHistory::CallStatistics::Monthly:
        group = groupTemplate.arg("%Y-%m");
        break;
    case CommHistory::CallStatistics::Weekly:
        group = groupTemplate.arg("%Y-%W");
        break;
    case CommHistory::CallStatistics::Daily:
        group = groupTemplate.arg("%Y-%m-%d");
        break;
    }

    QString q = "SELECT startTime, COUNT(*) from Events";
    if (!conditions.isEmpty()) {
        q += " WHERE " + conditions.join(" AND ");
    }

    return q + group;
}

CommHistory::CallStatistics::Result readNextResult(QSqlQuery *query)
{
    CommHistory::CallStatistics::Result result;
    if (!query->next()) {
        return result;
    }
    result.when = QDateTime::fromMSecsSinceEpoch(query->value(0).toLongLong() * 1000).toUTC();
    result.callCount = query->value(1).toInt();
    return result;
}

bool dateMatchesForInterval(const QDate &d1, const QDate &d2, CommHistory::CallStatistics::TimeInterval timeInterval)
{
    switch (timeInterval) {
    case CommHistory::CallStatistics::NoTimeInterval:
        return true;
    case CommHistory::CallStatistics::Yearly:
        return d1.year() == d2.year();
    case CommHistory::CallStatistics::Monthly:
        return d1.year() == d2.year() && d1.month() == d2.month();
    case CommHistory::CallStatistics::Weekly:
    {
        int d1YearNumber = 0;
        int d1WeekNumber = d1.weekNumber(&d1YearNumber);
        int d2YearNumber = 0;
        int d2WeekNumber = d2.weekNumber(&d2YearNumber);
        return d1YearNumber == d2YearNumber && d1WeekNumber == d2WeekNumber;
    }
    case CommHistory::CallStatistics::Daily:
        return d1 == d2;
    }
    return false;
}

QList<CommHistory::CallStatistics::Result> readQueryResults(CommHistory::CallStatistics::TimeInterval timeInterval,
                                                            const QDateTime &startTime,
                                                            const QDateTime &endTime,
                                                            QSqlQuery *query)
{
    QList<CommHistory::CallStatistics::Result> results;

    if (timeInterval == CommHistory::CallStatistics::NoTimeInterval) {
        CommHistory::CallStatistics::Result result = readNextResult(query);
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
    CommHistory::CallStatistics::Result rowResult;
    bool readNewResult = true;

    while (nextDate <= endDate) {
        if (readNewResult) {
            rowResult = readNextResult(query);
        }

        const QDateTime &rowDateTime = rowResult.when;
        const QDate &rowDate = rowDateTime.date();

        CommHistory::CallStatistics::Result newResult;
        if (dateMatchesForInterval(nextDate, startDate, timeInterval)) {
            // The first result's date is set to the query's start date.
            newResult.when = startTimeUtc;
        } else {
            // All following results are set to the beginning of the next entry's
            // year/month/week/day.
            newResult.when = QDateTime(nextDate, QTime(), Qt::UTC);
        }

        if (rowResult.when.isValid() && dateMatchesForInterval(nextDate, rowDate, timeInterval)) {
            // The db has entries for this year/month/week/day, so initialize the result according
            // to the db value.
            newResult.callCount = rowResult.callCount;
            readNewResult = true;
        } else {
            // No db entries were found for this year/month/week/day, so add an empty placeholder
            // and leave the db value for the next loop.
            newResult.callCount = 0;
            readNewResult = false;
        }

        results.append(newResult);

        // Set the start date for the next entry, i.e. the beginning of the next year/month/week/day.
        switch (timeInterval) {
        case CommHistory::CallStatistics::NoTimeInterval:
            break;
        case CommHistory::CallStatistics::Yearly:
            nextDate.setDate(nextDate.year() + 1, 1, 1);
            break;
        case CommHistory::CallStatistics::Monthly:
            nextDate = nextDate.addMonths(1);
            nextDate.setDate(nextDate.year(), nextDate.month(), 1);
            break;
        case CommHistory::CallStatistics::Weekly:
            nextDate = nextDate.addDays(Qt::Sunday - nextDate.dayOfWeek() + 1);
            break;
        case CommHistory::CallStatistics::Daily:
            nextDate = nextDate.addDays(1);
            break;
        }
    }

    return results;
}

}

namespace CommHistory {

CallStatisticsPrivate::CallStatisticsPrivate(CallStatistics *parent)
    : QObject(parent)
    , q(parent)
{
}

CallStatistics::CallStatistics(QObject *parent)
    : QObject(parent)
    , d(new CallStatisticsPrivate(this))
{
}

void CallStatistics::setStartTime(const QDateTime &dt)
{
    if (dt != d->startTime) {
        d->startTime = dt;
        emit startTimeChanged();
    }
}

QDateTime CallStatistics::startTime() const
{
    return d->startTime;
}

void CallStatistics::setEndTime(const QDateTime &dt)
{
    if (dt != d->endTime) {
        d->endTime = dt;
        emit endTimeChanged();
    }
}

QDateTime CallStatistics::endTime() const
{
    return d->endTime;
}

void CallStatistics::setCallType(CallEvent::CallType type)
{
    if (type != d->callType) {
        d->callType = type;
        emit callTypeChanged();
    }
}

CallEvent::CallType CallStatistics::callType() const
{
    return d->callType;
}

void CallStatistics::setTimeInterval(TimeInterval timeInterval)
{
    if (timeInterval != d->timeInterval) {
        d->timeInterval = timeInterval;
        emit timeIntervalChanged();
    }
}

CallStatistics::TimeInterval CallStatistics::timeInterval() const
{
    return d->timeInterval;
}

QList<CommHistory::CallStatistics::Result> CallStatistics::results() const
{
    return d->results;
}

bool CallStatistics::reload()
{
    d->results.clear();

    if (d->startTime.isValid() && d->endTime.isValid() && d->endTime <= d->startTime) {
        qWarning() << "Error: end time" << d->endTime.toString()
                   << "is not after start time" << d->startTime.toString();
        return false;
    }

    QString queryString = buildEventsQuery(d->callType, d->timeInterval, d->startTime, d->endTime);

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
