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

#ifndef COMMHISTORY_SMSHISTORY_H
#define COMMHISTORY_SMSHISTORY_H

#include <QObject>
#include <QDateTime>

#include "libcommhistoryexport.h"
#include "event.h"

namespace CommHistory {

class SMSHistoryPrivate;

class LIBCOMMHISTORY_EXPORT SMSHistory : public QObject
{
    Q_OBJECT

public:
    struct Result {
        QDateTime when;
        QString phoneNumber;
    };

    explicit SMSHistory(QObject *parent = 0);

    void setStartTime(const QDateTime &dt);
    QDateTime startTime() const;

    void setEndTime(const QDateTime &dt);
    QDateTime endTime() const;

    QList<CommHistory::SMSHistory::Result> results() const;

    bool reload();

Q_SIGNALS:
    void startTimeChanged();
    void endTimeChanged();

private:
    SMSHistoryPrivate *d;
};

}

#endif