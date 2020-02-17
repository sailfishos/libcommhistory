/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2020 D. Caliste.
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

#include "recipienteventmodel.h"

#include "databaseio_p.h"
#include "eventmodel_p.h"
#include "recipienteventmodel_p.h"

#include <QSqlQuery>

namespace CommHistory {

RecipientEventModelPrivate::RecipientEventModelPrivate(RecipientEventModel *model)
    : EventModelPrivate(model)
{
}

bool RecipientEventModelPrivate::acceptsEvent(const Event &event) const
{
    return m_recipients.intersects(event.recipients());
}

void RecipientEventModelPrivate::fetchEvents()
{
    if (!m_recipients.isEmpty()) {
        // Get the events that match these addresses
        QStringList clauses;
        QVariantList values;
        for (RecipientList::const_iterator it = m_recipients.constBegin();
            it != m_recipients.constEnd(); ++it) {
            if (it->localUid().isEmpty()) {
                clauses.append(QString("(remoteUid LIKE ? AND localUid LIKE '%1%%')").arg(RING_ACCOUNT));
                values.append(QString("%%%1%%").arg(minimizePhoneNumber(it->remoteUid())));
            } else {
                clauses.append("(remoteUid = ? AND localUid = ?)");
                values.append(it->remoteUid());
                values.append(it->localUid());
            }
        }

        QString where("WHERE ( ");
        where.append(clauses.join(" OR "));
        where.append(" ) ORDER BY Events.endTime DESC, Events.id DESC");

        QSqlQuery query = prepareQuery(DatabaseIOPrivate::eventQueryBase() + where);

        foreach (const QVariant &value, values)
            query.addBindValue(value);

        executeQuery(query);
    } else {
        modelUpdatedSlot(true);
    }
}

RecipientEventModel::RecipientEventModel(QObject *parent)
    : EventModel(*new RecipientEventModelPrivate(this), parent)
{
    setResolveContacts(ResolveImmediately);
}

RecipientEventModel::RecipientEventModel(RecipientEventModelPrivate &dd, QObject *parent)
    : EventModel(dd, parent)
{
    setResolveContacts(ResolveImmediately);
}

RecipientEventModel::~RecipientEventModel()
{
}

bool RecipientEventModel::getEvents(const RecipientList &recipients)
{
    Q_D(RecipientEventModel);

    beginResetModel();
    d->clearEvents();
    d->isReady = false;
    endResetModel();

    d->m_recipients = recipients;
    d->fetchEvents();
    return true;
}

bool RecipientEventModel::getEvents(const Recipient &recipient)
{
    return getEvents(RecipientList() << recipient);
}

} // namespace CommHistory

