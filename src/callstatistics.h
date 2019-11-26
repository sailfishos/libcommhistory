#ifndef COMMHISTORY_CALLSTATISTICS_H
#define COMMHISTORY_CALLSTATISTICS_H

#include <QObject>
#include <QDateTime>

#include "libcommhistoryexport.h"
#include "callevent.h"

namespace CommHistory {

class CallStatisticsPrivate;

class LIBCOMMHISTORY_EXPORT CallStatistics : public QObject
{
    Q_OBJECT

public:
    enum TimeInterval {
        NoTimeInterval,
        Yearly,
        Monthly,
        Weekly,
        Daily
    };
    Q_ENUM(TimeInterval)

    struct Result {
        QDateTime when;
        int callCount;
        Result() : callCount(0) {}
    };

    explicit CallStatistics(QObject *parent = 0);

    void setStartTime(const QDateTime &dt);
    QDateTime startTime() const;

    void setEndTime(const QDateTime &dt);
    QDateTime endTime() const;

    void setCallType(CallEvent::CallType type);
    CallEvent::CallType callType() const;

    void setTimeInterval(TimeInterval timeInterval);
    TimeInterval timeInterval() const;

    QList<CommHistory::CallStatistics::Result> results() const;

    bool reload();

Q_SIGNALS:
    void startTimeChanged();
    void endTimeChanged();
    void callTypeChanged();
    void timeIntervalChanged();

private:
    CallStatisticsPrivate *d;
};

}

#endif // COMMHISTORY_CALLSTATISTICS_H
