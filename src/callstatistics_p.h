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
    CallEvent::CallType callType = CallEvent::UnknownCallType;
    CallStatistics::TimeInterval timeInterval = CallStatistics::NoTimeInterval;
};

}
