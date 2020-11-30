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

#include "recipienteventmodel.h"

#include "databaseio_p.h"
#include "eventmodel_p.h"
#include "recipienteventmodel_p.h"

#include <QSqlQuery>

namespace CommHistory {

RecipientEventModelPrivate::RecipientEventModelPrivate(RecipientEventModel *model)
    : EventModelPrivate(model)
{
    connect(&m_fetcher, &ContactFetcher::finished,
            this, &RecipientEventModelPrivate::fetcherFinished);
}

bool RecipientEventModelPrivate::acceptsEvent(const Event &event) const
{
    if (m_contactId > 0) {
        return event.recipients().contactIds().contains(m_contactId);
    } else {
        // Use intersectsMatch() for non-exact match. E.g. to match normalized phone numbers
        return m_recipients.intersectsMatch(event.recipients());
    }
}

bool RecipientEventModelPrivate::fillModel(int start, int end, QList<CommHistory::Event> events, bool resolved)
{
    if (m_contactId > 0 || m_recipients.count()) {
        // Filter out any events that do not match our recipients
        QList<CommHistory::Event>::iterator it = events.begin();
        while (it != events.end()) {
            const Event &event(*it);
            if (acceptsEvent(event)) {
                ++it;
            } else {
                it = events.erase(it);
            }
        }
        return EventModelPrivate::fillModel(start, start + events.size(), events, resolved);
    }

    return EventModelPrivate::fillModel(start, end, events, resolved);
}

void RecipientEventModelPrivate::fetchEvents()
{
    if (!m_recipients.isEmpty()) {
        // Get the events that match these addresses
        QStringList clauses;
        QVariantList values;
        for (RecipientList::const_iterator it = m_recipients.constBegin();
            it != m_recipients.constEnd(); ++it) {
            if (CommHistory::localUidComparesPhoneNumbers(it->localUid())) {
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

void RecipientEventModelPrivate::fetcherFinished()
{
    Q_Q(RecipientEventModel);

    // Our contact is now loaded in the cache
    if (m_contactId > 0) {
        m_recipients = RecipientList::fromContact(m_contactId);
    } else if (m_recipients.count()) {
        RecipientList recipients = m_recipients;
        m_recipients = RecipientList();

        // If the recipients were resolved to a contact, add all the recipients of that contact to
        // m_recipients. This way, if a contact has e.g. multiple phone numbers, they will all be
        // added as candidates for the model.
        for (const Recipient &recipient : recipients) {
            if (recipient.contactId() > 0) {
                // The recipient was resolved to a contact. Add all of the contact's recipients to
                // the model.
                const RecipientList temp = RecipientList::fromContact(recipient.contactId());
                for (const Recipient &r : temp) {
                    m_recipients.append(r);
                }
            } else {
                // The recipient was not resolved to a contact, so add it directly.
                m_recipients.append(recipient);
            }
        }
    }

    fetchEvents();
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

void RecipientEventModel::setRecipients(const Recipient &recipient)
{
    Q_D(RecipientEventModel);

    setRecipients(RecipientList() << recipient);
}

void RecipientEventModel::setRecipients(const RecipientList &recipients)
{
    Q_D(RecipientEventModel);

    d->m_contactId = 0;
    d->m_recipients = recipients;
}

void RecipientEventModel::setRecipients(int contactId)
{
    Q_D(RecipientEventModel);

    d->m_contactId = contactId;
    d->m_recipients = RecipientList();
}

bool RecipientEventModel::getEvents()
{
    Q_D(RecipientEventModel);

    beginResetModel();
    d->clearEvents();
    d->isReady = false;
    endResetModel();

    if (d->m_contactId > 0) {
        d->m_fetcher.add(d->m_contactId);
        return true;

    } else if (!d->m_recipients.isEmpty()) {
        for (const Recipient &recipient : d->m_recipients) {
            d->m_fetcher.add(recipient);
        }

        return true;
    }

    return false;
}

} // namespace CommHistory
