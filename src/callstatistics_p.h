#ifndef CALLSTATISTICS_P_H
#define CALLSTATISTICS_P_H

#include "callstatistics.h"

namespace CommHistory {

class CallStatisticsPrivate: public QObject
{
    Q_OBJECT
public:
    explicit CallStatisticsPrivate(CallStatistics *parent);

    QList<CommHistory::CallStatistics::Result> results;
    CallStatistics *q;
    QDateTime startTime;
    QDateTime endTime;
    CallEvent::CallType callType;
    CallStatistics::TimeInterval timeInterval;
};

}

#endif
