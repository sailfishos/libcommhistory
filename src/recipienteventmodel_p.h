/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2020 D. Caliste.
** Copyright (C) 2020 Open Mobile Platform LLC.
** Contact: Damien Caliste <dcaliste@free.fr>
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

#ifndef COMMHISTORY_RECIPIENT_EVENT_MODEL_P_H
#define COMMHISTORY_RECIPIENT_EVENT_MODEL_P_H

#include "recipienteventmodel.h"
#include "eventmodel_p.h"
#include "contactfetcher.h"

namespace CommHistory {

class LIBCOMMHISTORY_EXPORT RecipientEventModelPrivate : public EventModelPrivate
{
    Q_OBJECT

    Q_DECLARE_PUBLIC(RecipientEventModel)

public:
    RecipientEventModelPrivate(RecipientEventModel *model = 0);

    bool acceptsEvent(const Event &event) const override;
    bool fillModel(int start, int end, QList<CommHistory::Event> events, bool resolved) override;
    void fetchEvents();

    ContactFetcher m_fetcher;
    RecipientList m_recipients;
    int m_contactId = 0;

public slots:
    void fetcherFinished();
};

}

#endif
