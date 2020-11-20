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

#ifndef CALLHISTORY_P_H
#define CALLHISTORY_P_H

#include "callhistory.h"

namespace CommHistory {

class CallHistoryPrivate: public QObject
{
    Q_OBJECT
public:
    explicit CallHistoryPrivate(CallHistory *parent);

    QList<CommHistory::CallHistory::Result> results;
    CallHistory *q;
    QDateTime startTime;
    QDateTime endTime;
    CallEvent::CallType callType;
};

}

#endif
