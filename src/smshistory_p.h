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

#ifndef COMMHISTORY_SMSHISTORY_P_H
#define COMMHISTORY_SMSHISTORY_P_H

#include "smshistory.h"

namespace CommHistory {

class SMSHistoryPrivate: public QObject
{
    Q_OBJECT
public:
    explicit SMSHistoryPrivate(SMSHistory *parent);

    QList<CommHistory::SMSHistory::Result> results;
    SMSHistory *q;
    QDateTime startTime;
    QDateTime endTime;
    SMSHistory::TimeInterval timeInterval;
};

}

#endif
