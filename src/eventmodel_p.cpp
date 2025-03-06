/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd. <john.brooks@jollamobile.com>
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@nokia.com>
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

#include <QtDBus/QtDBus>
#include <QSqlQuery>
#include <QSqlError>

#include "databaseio.h"
#include "databaseio_p.h"
#include "eventmodel.h"
#include "eventmodel_p.h"
#include "updatesemitter.h"
#include "dbus_p.h"
#include "event.h"
#include "eventtreeitem.h"
#include "commonutils.h"
#include "debug_p.h"

using namespace CommHistory;

namespace {

bool initializeTypes()
{
    qRegisterMetaType<QList<CommHistory::Event> >();
    return true;
}

const int defaultChunkSize = 50;

}

bool eventmodel_p_initialized = initializeTypes();

EventModelPrivate::EventModelPrivate(EventModel *model)
        : addResolver(0)
        , receiveResolver(0)
        , onDemandResolver(0)
        , queryMode(EventModel::AsyncQuery)
        , chunkSize(defaultChunkSize)
        , firstChunkSize(0)
        , queryLimit(0)
        , queryOffset(0)
        , eventCategoryMask(Event::AnyCategory)
        , isInTreeMode(false)
        , isReady(true)
        , accept(false)
        , threadCanFetchMore(false)
        , bufferInsertions(false)
        , resolveContacts(EventModel::DoNotResolve)
        , propertyMask(Event::allProperties())
        , bgThread(0)
{
    q_ptr = model;

    // emit dbus signals
    emitter = UpdatesEmitter::instance();
    connect(this, SIGNAL(eventsAdded(const QList<CommHistory::Event>&)),
            emitter.data(), SIGNAL(eventsAdded(const QList<CommHistory::Event>&)));
    connect(this, SIGNAL(eventsUpdated(const QList<CommHistory::Event>&)),
            emitter.data(), SIGNAL(eventsUpdated(const QList<CommHistory::Event>&)));
    connect(this, SIGNAL(eventDeleted(int)),
            emitter.data(), SIGNAL(eventDeleted(int)));
    connect(this, SIGNAL(groupsUpdated(const QList<int>&)),
            emitter.data(), SIGNAL(groupsUpdated(const QList<int>&)));
    connect(this, SIGNAL(groupsUpdatedFull(const QList<CommHistory::Group>&)),
            emitter.data(), SIGNAL(groupsUpdatedFull(const QList<CommHistory::Group>&)));
    connect(this, SIGNAL(groupsDeleted(const QList<int>&)),
            emitter.data(), SIGNAL(groupsDeleted(const QList<int>&)));

    // listen to dbus signals
    QDBusConnection::sessionBus().connect(
        QString(), QString(), COMM_HISTORY_INTERFACE, EVENTS_ADDED_SIGNAL,
        this, SLOT(eventsAddedSlot(const QList<CommHistory::Event> &)));
    QDBusConnection::sessionBus().connect(
        QString(), QString(), COMM_HISTORY_INTERFACE, EVENTS_UPDATED_SIGNAL,
        this, SLOT(eventsUpdatedSlot(const QList<CommHistory::Event> &)));
    QDBusConnection::sessionBus().connect(
        QString(), QString(), COMM_HISTORY_INTERFACE, EVENT_DELETED_SIGNAL,
        this, SLOT(eventDeletedSlot(int)));

    eventRootItem = new EventTreeItem(Event());
}

EventModelPrivate::~EventModelPrivate()
{
    qCDebug(lcCommHistory) << Q_FUNC_INFO;

    delete eventRootItem;
}

bool EventModelPrivate::acceptsEvent(const Event &event) const
{
    if (eventCategoryMask != Event::AnyCategory) {
        return event.category() & eventCategoryMask;
    }
    return accept;
}

QModelIndex EventModelPrivate::findEventRecursive(int id, EventTreeItem *parent) const
{
    Q_Q(const EventModel);

    if (id < 0)
        return QModelIndex();

    for (int row = 0; row < parent->childCount(); row++) {
        if (parent->eventAt(row).id() == id) {
            return q->createIndex(row, 0, parent->child(row));
        } else if (parent->child(row)->childCount()) {
            return findEventRecursive(id, parent->child(row));
        }
    }
    return QModelIndex();
}

QModelIndex EventModelPrivate::findEvent(int id) const
{
    return findEventRecursive(id, eventRootItem);
}

QSqlQuery EventModelPrivate::prepareQuery(const QString &q) const
{
    if (queryLimit > 0 || queryOffset > 0) {
        return DatabaseIOPrivate::prepareQuery(q, queryLimit, queryOffset);
    }
    return DatabaseIOPrivate::prepareQuery(q);
}

QSqlQuery EventModelPrivate::prepareQuery(const QString &q, int limit, int offset) const
{
    return DatabaseIOPrivate::prepareQuery(q, limit, offset);
}

bool EventModelPrivate::executeQuery(QSqlQuery &query)
{
    qCDebug(lcCommHistory) << Q_FUNC_INFO;

    isReady = false;

    if (!query.exec()) {
        qCWarning(lcCommHistory) << "Failed to execute query";
        qCWarning(lcCommHistory) << query.lastError();
        qCWarning(lcCommHistory) << query.lastQuery();
        return false;
    }

    QList<Event> events;
    QList<int> extraPropertyIndices;
    QList<int> hasPartsIndices;
    while (query.next()) {
        Event e;
        bool extra = false, parts = false;
        DatabaseIOPrivate::readEventResult(query, e, extra, parts);
        if (extra)
            extraPropertyIndices.append(events.size());
        if (parts)
            hasPartsIndices.append(events.size());
        events.append(e);
    }
    query.finish();

    foreach (int i, extraPropertyIndices)
        DatabaseIO::instance()->getEventExtraProperties(events[i]);
    foreach (int i, hasPartsIndices)
        DatabaseIO::instance()->getMessageParts(events[i]);

    eventsReceivedSlot(0, events.size(), events);
    return true;
}

bool EventModelPrivate::fillModel(int start, int end, QList<CommHistory::Event> events, bool resolved)
{
    Q_UNUSED(start);
    Q_UNUSED(end);
    Q_UNUSED(resolved);

    if (events.isEmpty()) {
        // Empty results are still "ready"
        modelUpdatedSlot(true);
        return true;
    }

    Q_Q(EventModel);
    qCDebug(lcCommHistory) << Q_FUNC_INFO << ": read" << events.count() << "events";

    q->beginInsertRows(QModelIndex(), q->rowCount(), q->rowCount() + events.count() - 1);
    foreach (const Event &event, events) {
        eventRootItem->appendChild(new EventTreeItem(event, eventRootItem));
    }
    q->endInsertRows();

    modelUpdatedSlot(true);
    return true;
}

bool EventModelPrivate::fillModel(QList<CommHistory::Event> events, bool resolved)
{
    Q_Q(EventModel);

    QMutableListIterator<Event> i(events);
    while (i.hasNext()) {
        const Event &event = i.next();
        if (findEvent(event.id()).isValid()) {
            i.remove();
            continue;
        }
    }

    if (events.isEmpty()) {
        // Empty results are still "ready"
        modelUpdatedSlot(true);
        return true;
    } else {
        return fillModel(q->rowCount(), q->rowCount() + events.count() - 1, events, resolved);
    }


    return true;
}

void EventModelPrivate::clearEvents()
{
    qCDebug(lcCommHistory) << Q_FUNC_INFO;
    delete eventRootItem;
    eventRootItem = new EventTreeItem(Event());
}

void EventModelPrivate::setBufferInsertions(bool buffer)
{
    if (bufferInsertions != buffer) {
        bufferInsertions = buffer;

        if (!bufferInsertions && !bufferedInsertions.isEmpty()) {
            // Add the events that were previously buffered
            addToModel(bufferedInsertions, true);
            bufferedInsertions.clear();
        }
    }
}

void EventModelPrivate::addToModel(const QList<Event> &events, bool sync)
{
    qCDebug(lcCommHistory) << Q_FUNC_INFO << events.count();

    if (bufferInsertions) {
        // Queue events for later processing
        bufferedInsertions.append(events);
        return;
    }

    // Insert immediately if synchronous (and resolve contact later if necessary),
    // or if not resolving contacts
    if (sync || resolveContacts != EventModel::ResolveImmediately)
        prependEvents(events, false);

    // Resolve contacts. If inserted above, the duplicate will be ignored
    if (resolveContacts == EventModel::ResolveImmediately)
        resolveAddedEvents(events);
}

void EventModelPrivate::resolveAddedEvents(const QList<Event> &events)
{
    if (!addResolver) {
        addResolver = new ContactResolver(this);
        connect(addResolver, SIGNAL(finished()), SLOT(addResolverFinished()));
    }

    pendingAdded.append(events);
    addResolver->add(events);
}

void EventModelPrivate::addResolverFinished()
{
    QList<Event> resolved(pendingAdded);
    pendingAdded.clear();

    QList<Event>::iterator it = resolved.begin(), end = resolved.end();
    for ( ; it != end; ++it) {
        Event &event(*it);
        if (!event.isResolved() && event.recipients().allContactsResolved())
            event.setIsResolved(true);
    }

    prependEvents(resolved, true);
}

void EventModelPrivate::prependEvents(QList<Event> events, bool resolved)
{
    Q_UNUSED(resolved);
    Q_Q(EventModel);

    // Replace exact duplicates instead of inserting. This is a workaround
    // for the sync mode in addToModel.
    for (int i = 0; i < events.size(); i++) {
        for (int j = 0; j < eventRootItem->childCount(); j++) {
            if (eventRootItem->eventAt(j) == events[i]) {
                eventRootItem->child(j)->setEvent(events[i]);
                emitDataChanged(j, eventRootItem->child(j));
                events.removeAt(i);
                i--;
                break;
            }
        }
    }

    if (events.isEmpty())
        return;

    q->beginInsertRows(QModelIndex(), 0, events.size() - 1);
    for (int i = events.size() - 1; i >= 0; i--) {
        eventRootItem->prependChild(new EventTreeItem(events[i], eventRootItem));
    }
    q->endInsertRows();
}

void EventModelPrivate::resolveIfRequired(const Event &event) const
{
    if (resolveContacts != EventModel::ResolveOnDemand || event.isResolved())
        return;

    if (!onDemandResolver) {
        onDemandResolver = new ContactResolver(const_cast<EventModelPrivate *>(this));
        connect(onDemandResolver, SIGNAL(finished()), SLOT(onDemandResolverFinished()));
    }

    pendingOnDemand.append(event);
    onDemandResolver->add(event);
}

void EventModelPrivate::onDemandResolverFinished()
{
    QList<Event> resolved(pendingOnDemand);
    pendingOnDemand.clear();

    QSet<Recipient> resolvedRecipients;
    foreach (const Event &event, resolved) {
        const RecipientList &recipients(event.recipients());
        for (RecipientList::const_iterator it = recipients.constBegin(), end = recipients.constEnd(); it != end; ++it)
            resolvedRecipients.insert(*it);
    }

    slotContactChanged(resolvedRecipients.values());
}

void EventModelPrivate::modifyInModel(Event &event)
{
    Q_Q(EventModel);
    qCDebug(lcCommHistory) << Q_FUNC_INFO << event.id();

    QModelIndex index = findEvent(event.id());
    if (index.isValid()) {
        EventTreeItem *item = static_cast<EventTreeItem *>(index.internalPointer());
        Event oldEvent = item->event();
        quint32 oldTimeT = oldEvent.endTimeT();
        oldEvent.copyValidProperties(event);
        item->setEvent(oldEvent);

        // move event if endTime has changed
        const int row(index.row());
        if (row > 0 && oldTimeT < event.endTimeT()) {
            EventTreeItem *parent = item->parent();
            if (!parent)
                parent = eventRootItem;

            if (parent == eventRootItem) {
                q->beginMoveRows(index.parent(), row, row, index.parent(), 0);
                parent->moveChild(row, 0);
                q->endMoveRows();
            } else {
                emit q->layoutAboutToBeChanged();
                parent->moveChild(row, 0);
                emit q->layoutChanged();
            }
        } else {
            emitDataChanged(row, index.internalPointer());
        }
    }
}

void EventModelPrivate::deleteFromModel(int id)
{
    Q_Q(EventModel);
    qCDebug(lcCommHistory) << Q_FUNC_INFO << id;
    QModelIndex index = findEvent(id);
    if (index.isValid()) {
        q->beginRemoveRows(index.parent(), index.row(), index.row());
        EventTreeItem *parent = static_cast<EventTreeItem *>(index.parent().internalPointer());
        if (!parent) parent = eventRootItem;
        parent->removeAt(index.row());
        q->endRemoveRows();
    }
}

void EventModelPrivate::eventsReceivedSlot(int start, int end, QList<Event> events)
{
    qCDebug(lcCommHistory) << Q_FUNC_INFO << ":" << start << end << events.count();

    if (events.isEmpty()) {
        // Empty results are still "ready"
        modelUpdatedSlot(true);
        return;
    }

    // Contact resolution is not allowed in synchronous query mode
    if (resolveContacts == EventModel::ResolveImmediately && queryMode != EventModel::SyncQuery) {
        if (!receiveResolver) {
            receiveResolver = new ContactResolver(this);
            connect(receiveResolver, SIGNAL(finished()), SLOT(receiveResolverFinished()));
        }

        pendingReceived.append(events);
        receiveResolver->add(events);
    } else {
        fillModel(start, end, events, false);
    }
}

void EventModelPrivate::receiveResolverFinished()
{
    QList<Event> resolved(pendingReceived);
    pendingReceived.clear();

    QList<Event>::iterator it = resolved.begin(), end = resolved.end();
    for ( ; it != end; ++it) {
        Event &event(*it);
        if (!event.isResolved() && event.recipients().allContactsResolved())
            event.setIsResolved(true);
    }

    fillModel(resolved, true);
}

void EventModelPrivate::modelUpdatedSlot(bool successful)
{
    qCDebug(lcCommHistory) << Q_FUNC_INFO;

    isReady = true;
    emit modelReady(successful);
}

void EventModelPrivate::eventsAddedSlot(const QList<Event> &events)
{
    qCDebug(lcCommHistory) << Q_FUNC_INFO << ":" << events.count() << "events";

    foreach (const Event &event, events) {
        QModelIndex index = findEvent(event.id());
        if (index.isValid()) return;

        Event e = event;
        if (acceptsEvent(e))
            addToModel(e);
    }
}

void EventModelPrivate::eventsUpdatedSlot(const QList<Event> &events)
{
    qCDebug(lcCommHistory) << Q_FUNC_INFO << ":" << events.count();

    foreach (const Event &event, events) {
        QModelIndex index = findEvent(event.id());
        Event e = event;

        if (!index.isValid()) {
            if (acceptsEvent(e))
                addToModel(e);

            continue;
        }

        modifyInModel(e);
    }
}

void EventModelPrivate::eventDeletedSlot(int id)
{
    qCDebug(lcCommHistory) << Q_FUNC_INFO << ":" << id;

    deleteFromModel(id);
}

void EventModelPrivate::canFetchMoreChangedSlot(bool canFetch)
{
    threadCanFetchMore = canFetch;
}

bool EventModelPrivate::canFetchMore() const
{
    return threadCanFetchMore;
}

void EventModelPrivate::recipientsChangedRecursive(const QSet<Recipient> &recipients, EventTreeItem *parent, bool resolved)
{
    for (int row = 0; row < parent->childCount(); row++) {
        const Event &event(parent->eventAt(row));
        EventTreeItem *child = parent->child(row);
        if (event.recipients().intersects(recipients)) {
            if (resolved) {
                Event &event(child->event());
                if (!event.isResolved() && event.recipients().allContactsResolved())
                    event.setIsResolved(true);
            }

            // XXX coalesce
            // XXX role dataChanged signal
            emitDataChanged(row, child);
        }
        if (child->childCount())
            recipientsChangedRecursive(recipients, child, resolved);
    }
}

void EventModelPrivate::recipientsUpdated(const QSet<Recipient> &recipients, bool resolved)
{
    recipientsChangedRecursive(recipients, eventRootItem, resolved);
}

void EventModelPrivate::slotContactInfoChanged(const RecipientList &recipients)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QSet<Recipient> changed = QSet<Recipient>(recipients.recipients().begin(), recipients.recipients().end());
#else
    QSet<Recipient> changed = QSet<Recipient>::fromList(recipients.recipients());
#endif
    recipientsUpdated(changed);
}

void EventModelPrivate::slotContactChanged(const RecipientList &recipients)
{
#if QT_VERSION > QT_VERSION_CHECK(5, 15, 0)
    QSet<Recipient> changed = QSet<Recipient>(recipients.recipients().begin(), recipients.recipients().end());
#else
    QSet<Recipient> changed = QSet<Recipient>::fromList(recipients.recipients());
#endif
    recipientsUpdated(changed, true);
}

void EventModelPrivate::slotContactDetailsChanged(const RecipientList &recipients)
{
    Q_UNUSED(recipients)
}

DatabaseIO* EventModelPrivate::database()
{
    return DatabaseIO::instance();
}

void EventModelPrivate::setResolveContacts(EventModel::ContactResolveType type)
{
    if (resolveContacts == type)
        return;

    resolveContacts = type;
    if (resolveContacts != EventModel::DoNotResolve && !contactListener) {
        contactListener = ContactListener::instance();
        connect(contactListener.data(),
                SIGNAL(contactInfoChanged(RecipientList)),
                SLOT(slotContactInfoChanged(RecipientList)));
        connect(contactListener.data(),
                SIGNAL(contactChanged(RecipientList)),
                SLOT(slotContactChanged(RecipientList)));
        connect(contactListener.data(),
                SIGNAL(contactDetailsChanged(RecipientList)),
                SLOT(slotContactDetailsChanged(RecipientList)));
    } else if (resolveContacts == EventModel::DoNotResolve && contactListener) {
        disconnect(contactListener.data(), 0, this, 0);
        contactListener.clear();

        delete receiveResolver;
        receiveResolver = 0;

        delete onDemandResolver;
        onDemandResolver = 0;
    }
}

void EventModelPrivate::emitDataChanged(int row, void *data)
{
    Q_Q(EventModel);

    const QModelIndex modelIndex(q->createIndex(row, 0, data));
    emit q->dataChanged(modelIndex, modelIndex);
}

