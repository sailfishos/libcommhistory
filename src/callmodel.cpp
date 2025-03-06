/******************************************************************************
**
** This file is part of libcommhistory.
**
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
#include "callmodel.h"
#include "event.h"
#include "debug_p.h"

namespace {
CommHistory::Event::PropertySet unusedProperties = CommHistory::Event::PropertySet()
        << CommHistory::Event::IsDraft
        << CommHistory::Event::FreeText
        << CommHistory::Event::MessageToken
        << CommHistory::Event::ReportDelivery
        << CommHistory::Event::ValidityPeriod
        << CommHistory::Event::ContentLocation
        << CommHistory::Event::FromVCardFileName
        << CommHistory::Event::FromVCardLabel
        << CommHistory::Event::MessageParts
        << CommHistory::Event::ReadStatus
        << CommHistory::Event::ReportRead
        << CommHistory::Event::ReportReadRequested
        << CommHistory::Event::MmsId;
}

namespace CommHistory
{

using namespace CommHistory;

/* ************************************************************************** *
 * ******** P R I V A T E   C L A S S   I M P L E M E N T A T I O N ********* *
 * ************************************************************************** */

class CallModelPrivate : public EventModelPrivate
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(CallModel)

public:
    CallModelPrivate(EventModel *model);

    void eventsReceivedSlot(int start, int end, QList<CommHistory::Event> events);
    void modelUpdatedSlot(bool successful);
    bool eventMatchesFilter(const Event &event) const;
    bool acceptsEvent(const Event &event) const;
    int calculateEventCount(EventTreeItem *item);
    bool fillModel(int start, int end, QList<CommHistory::Event> events, bool resolved);
    bool belongToSameGroup(const Event &e1, const Event &e2);
    void prependEvents(QList<Event> events, bool resolved);
    void insertEvent(Event event);
    void eventsAddedSlot(const QList<Event> &events);
    void eventsUpdatedSlot(const QList<Event> &events);
    QModelIndex findEvent(int id) const;
    void deleteFromModel(int id);

    virtual void recipientsUpdated(const QSet<Recipient> &recipients, bool resolved = false);

public Q_SLOTS:
    void slotAllCallsDeleted(int unused);

public:
    CallModel::Sorting sortBy;
    CallEvent::CallType eventType;
    quint32 referenceTime;
    QString filterLocalUid;
    bool hasBeenFetched;
    QSet<QString> countedUids;
    QSet<QString> updatedGroups;
};

CallModelPrivate::CallModelPrivate(EventModel *model)
        : EventModelPrivate(model)
        , sortBy(CallModel::SortByContact)
        , eventType(CallEvent::UnknownCallType)
        , referenceTime(0)
        , hasBeenFetched(false)
{
    propertyMask -= unusedProperties;
}

bool CallModelPrivate::eventMatchesFilter(const Event &event) const
{
    bool match = true;

    switch (eventType) {
    case CallEvent::MissedCallType:
        if (event.direction() != Event::Inbound || !event.isMissedCall())
            match = false;
        break;
    case CallEvent::DialedCallType:
        if (event.direction() != Event::Outbound)
            match = false;
        break;
    case CallEvent::ReceivedCallType:
        if (event.direction() != Event::Inbound || event.isMissedCall())
            match = false;
        break;
    default:
        break;
    }

    if (!filterLocalUid.isEmpty() && event.localUid() != filterLocalUid)
        match = false;

    return match;
}

bool CallModelPrivate::acceptsEvent(const Event &event) const
{
    qCDebug(lcCommHistory) << Q_FUNC_INFO << event.id();
    if (event.type() != Event::CallEvent || !eventMatchesFilter(event)) {
        return false;
    }

    // a reference Time is already set, so any further event addition should be beyond that
    if (referenceTime != 0 && (event.startTimeT() < referenceTime)) {
        return false;
    }

    return true;
}

void CallModelPrivate::eventsReceivedSlot(int start, int end, QList<CommHistory::Event> events)
{
    Q_Q(CallModel);

    qCDebug(lcCommHistory) << Q_FUNC_INFO << start << end << events.count();

    if ((sortBy != CallModel::SortByContact && sortBy != CallModel::SortByContactAndType)
            || updatedGroups.isEmpty())
        return EventModelPrivate::eventsReceivedSlot(start, end, events);

    // reimp from EventModelPrivate, for video calls

    // Here we should usually get one or two result rows, one for the
    // video call group and one for the corresponding audio call group.
    QMutableListIterator<Event> i(events);
    while (i.hasNext()) {
        Event event = i.next();

        bool replaced = false;
        QModelIndex index;
        for (int row = 0; row < eventRootItem->childCount(); row++) {
            const Event &rowEvent(eventRootItem->eventAt(row));
            if (rowEvent.id() == event.id()
                || belongToSameGroup(rowEvent, event)) {
                qCDebug(lcCommHistory) << "replacing row" << row;
                replaced = true;
                eventRootItem->child(row)->setEvent(event);
                emitDataChanged(row, eventRootItem->child(row));
                updatedGroups.remove(DatabaseIOPrivate::makeCallGroupURI(event));

                // if we had an audio and video call group for the same
                // contact and the latest audio call gets upgraded (or
                // vice versa), there may now be two rows for the same
                // group, so we have to remove the other one.
                for (int dupe = row + 1; dupe < eventRootItem->childCount(); dupe++) {
                    const Event &e = eventRootItem->eventAt(dupe);
                    if (belongToSameGroup(e, event)) {
                        qCDebug(lcCommHistory) << Q_FUNC_INFO << "remove" << dupe << e.toString();
                        emit q->beginRemoveRows(QModelIndex(), dupe, dupe);
                        eventRootItem->removeAt(dupe);
                        emit q->endRemoveRows();
                        break;
                    }
                }
                break;
            }
        }

        if (!replaced) {
            int row;
            for (row = 0; row < eventRootItem->childCount(); row++) {
                if (eventRootItem->child(row)->event().endTimeT() <= event.endTimeT())
                    break;
            }

            q->beginInsertRows(QModelIndex(), row, row);
            eventRootItem->insertChildAt(row, new EventTreeItem(event, eventRootItem));
            q->endInsertRows();

            updatedGroups.remove(DatabaseIOPrivate::makeCallGroupURI(event));
        }
    }

    if (!updatedGroups.isEmpty()) {
        qCDebug(lcCommHistory) << Q_FUNC_INFO << "remaining call groups:" << updatedGroups;
        // no results for call group means it has been emptied, remove from list
        foreach (QString group, updatedGroups.values()) {
            for (int row = 0; row < eventRootItem->childCount(); row++) {
                if (DatabaseIOPrivate::makeCallGroupURI(eventRootItem->eventAt(row)) == group) {
                    qCDebug(lcCommHistory) << Q_FUNC_INFO << "remove" << row << eventRootItem->eventAt(row).toString();
                    emit q->beginRemoveRows(QModelIndex(), row, row);
                    eventRootItem->removeAt(row);
                    emit q->endRemoveRows();
                    break;
                }
            }
        }
    }
}

void CallModelPrivate::modelUpdatedSlot(bool successful)
{
    EventModelPrivate::modelUpdatedSlot(successful);
    countedUids.clear();
    updatedGroups.clear();
}

bool CallModelPrivate::belongToSameGroup(const Event &e1, const Event &e2)
{
    if (sortBy == CallModel::SortByContact
        && (e1.isResolved() && e2.isResolved() ? e1.recipients().hasSameContacts(e2.recipients())
                                               : e1.recipients().matches(e2.recipients()))
        && e1.isVideoCall() == e2.isVideoCall()) {
        return true;
    } else if ((sortBy == CallModel::SortByTime || sortBy == CallModel::SortByContactAndType)
             && (e1.direction() == e2.direction()
                 && e1.isMissedCall() == e2.isMissedCall()
                 && (e1.isResolved() && e2.isResolved() ? e1.recipients().hasSameContacts(e2.recipients())
                                                        : e1.recipients().matches(e2.recipients()))
                 && e1.isVideoCall() == e2.isVideoCall())) {
        return true;
    }
    return false;
}

int CallModelPrivate::calculateEventCount(EventTreeItem *item)
{
    int count = -1;

    switch (sortBy)
    {
        case CallModel::SortByContact:
        case CallModel::SortByContactAndType:
        {
            // set event count for missed calls only,
            // leave the default value for non-missed ones
            if (item->event().isMissedCall())
            {
                count = 1;
                // start looping the list from index number 1, because
                // the index number 0 is the same item as the top level
                // one
                for (int i = 1; i < item->childCount(); i++) {
                    if (item->event().incomingStatus()
                        == item->child(i)->event().incomingStatus()) {
                        count++;
                    } else {
                        break;
                    }
                }
            }
            return (count < 0) ? 0 : count;
        }
        case CallModel::SortByTime:
        {
            count = item->childCount();
            break;
        }
        default:
            break;
    }

    if (count < 1)
        count = 1;

    return count;
}

bool CallModelPrivate::fillModel(int start, int end, QList<CommHistory::Event> events, bool resolved)
{
    Q_Q(CallModel);

    //for flat mode EventModelPrivate::fillModel is sufficient as all the events will be stored at the top level
    if (!isInTreeMode) {
        return EventModelPrivate::fillModel(start, end, events, resolved);
    }

    if (events.count() > 0) {
        /*
         * call events are grouped as follows:
         *
         * [event 1] - (event 1)
         *             (event 2)
         *             (event 3)
         * [event 4] - (event 4)
         *             (event 5)
         * ...         ...
         *
         * NOTE:
         * on the top level, there are only the representatives of each group
         * on second level, there are all call events listed (also group reps)
         */

        switch (sortBy)
        {
            /*
             * if sorted by contact,
             * then event count is meaningful only for missed calls.
             * it shows how many missed calls there are under the top
             * level one without breaking the continuity of the missed
             * calls.
             *
             * John, 3 missed
             * Mary, 1 received
             * John, 1 dialed
             * John, 1 missed
             * Mary, 2 dialed
             *
             *      ||
             *      \/
             *
             * John, 3 missed
             * Mary, received
             *
             * NOTE 1:
             * there are actually 4 missed calls from John, but there was
             * 1 dialed in between, that is why the event count is 3.
             *
             * NOTE 2:
             * there is no number for the received calls, since only the
             * missed calls have valid even count. (But -1 will be returned.)
             */
            case CallModel::SortByContact:
            case CallModel::SortByContactAndType:
            {
                QList<EventTreeItem *> topLevelItems;
                // get the first event and save it as top level item
                Event event = events.first();
                EventTreeItem *parent = new EventTreeItem(event);
                topLevelItems.append(parent);
                parent->appendChild(new EventTreeItem(event, parent));

                for (int i = 1; i < events.count(); i++) {
                    const Event &event = events.at(i);

                    int j = 0;
                    for (; j < topLevelItems.count(); ++j) {
                        EventTreeItem *topLevelItem = topLevelItems.at(j);

                        // ignore matching events because the already existing
                        // entry has to be more recent
                        if (belongToSameGroup(topLevelItem->event(), event)) {
                            topLevelItem->appendChild(new EventTreeItem(event, topLevelItem));
                            break;
                        }
                    }
                    if (j == topLevelItems.count()) {
                        parent = new EventTreeItem(event);
                        topLevelItems.append(parent);
                        parent->appendChild(new EventTreeItem(event, parent));
                    }
                }

                // save top level items into the model
                q->beginInsertRows(QModelIndex(), 0, topLevelItems.count() - 1);
                foreach (EventTreeItem *item, topLevelItems) {
                    item->event().setEventCount(calculateEventCount(item));
                    eventRootItem->appendChild(item);
                }
                q->endInsertRows();

                break;
            }
            /*
             * if sorted by time,
             * then event count is the number of events grouped under the
             * top level one
             *
             * John, 3 missed
             * Mary, 1 received
             * John, 1 dialed
             * John, 1 missed
             * Mary, 2 dialed
             */
            case CallModel::SortByTime:
            {
                int previousLastRow = eventRootItem->childCount() - 1;
                EventTreeItem *previousLastItem = 0;

                EventTreeItem *last = 0;
                if (eventRootItem->childCount()) {
                    last = eventRootItem->child(previousLastRow);
                    previousLastItem = last;
                }

                QList<EventTreeItem *> newItems;

                foreach (Event event, events) {
                    if (last && last->event().eventCount() == -1
                        && belongToSameGroup(event, last->event())) {
                        // still filling last row with matching events
                        last->appendChild(new EventTreeItem(event, last));
                    } else {
                        // no match to previous event -> update count
                        // for last row and add a new row if event is
                        // acceptable
                        if (last) {
                            const QString shortNumber(last->event().recipients().value(0).minimizedRemoteUid());
                            if (!countedUids.contains(shortNumber)) {
                                last->event().setEventCount(calculateEventCount(last));
                                countedUids.insert(shortNumber);
                            }

                            if (last != previousLastItem && eventMatchesFilter(last->event())) {
                                newItems.append(last);
                            } else if (last != previousLastItem) {
                                delete last;
                                last = 0;
                            }
                        }

                        event.setEventCount(-1);
                        last = new EventTreeItem(event);
                        last->appendChild(new EventTreeItem(event, last));
                    }
                }

                if (last && last != previousLastItem && eventMatchesFilter(last->event())) {
                    const QString shortNumber(last->event().recipients().value(0).minimizedRemoteUid());
                    if (!countedUids.contains(shortNumber)) {
                        last->event().setEventCount(calculateEventCount(last));
                        countedUids.insert(shortNumber);
                    }
                    newItems.append(last);
                } else if (last != previousLastItem) {
                    delete last;
                }

                // update count for last item in the previous batch
                if (!newItems.isEmpty()) {
                    if (previousLastRow != -1)
                        emitDataChanged(previousLastRow, eventRootItem->child(previousLastRow));

                    // insert the rest
                    q->beginInsertRows(QModelIndex(), previousLastRow + 1, previousLastRow + newItems.count());
                    foreach (EventTreeItem *item, newItems)
                        eventRootItem->appendChild(item);
                    q->endInsertRows();
                }

                break;
            }

            default:
                break;
        }
    }

    modelUpdatedSlot(true);
    return true;
}

void CallModelPrivate::prependEvents(QList<Event> events, bool resolved)
{
    if (!isInTreeMode) {
        EventModelPrivate::prependEvents(events, resolved);
        return;
    }

    foreach (const Event &event, events)
        insertEvent(event);
}

void CallModelPrivate::insertEvent(Event event)
{
    Q_Q(CallModel);
    qCDebug(lcCommHistory) << Q_FUNC_INFO << event.toString();

    switch (sortBy) {
        case CallModel::SortByContact:
        case CallModel::SortByContactAndType:
        {
            // find match, update count if needed, move to top
            int matchingRow = -1;
            for (int i = 0; i < eventRootItem->childCount(); i++) {
                if (belongToSameGroup(eventRootItem->child(i)->event(), event)) {
                    matchingRow = i;
                    break;
                }
            }

            if (matchingRow != -1) {
                EventTreeItem *matchingItem = eventRootItem->child(matchingRow);

                const int groupEventCount(matchingItem->event().eventCount());
                const bool increaseEventCount(matchingItem->event().direction() == event.direction() &&
                                              matchingItem->event().incomingStatus() == event.incomingStatus());

                matchingItem->prependChild(new EventTreeItem(event, matchingItem));
                matchingItem->setEvent(event);
                matchingItem->event().setEventCount(increaseEventCount ? groupEventCount + 1 : 1);

                if (matchingRow != 0) {
                    // move to top
                    q->beginMoveRows(QModelIndex(), matchingRow, matchingRow, QModelIndex(), 0);
                    eventRootItem->moveChild(matchingRow, 0);
                    q->endMoveRows();
                }

                // update row data
                emitDataChanged(0, matchingItem);
            } else {
                // no match, insert new row at top
                emit q->beginInsertRows(QModelIndex(), 0, 0);
                event.setEventCount(1);

                EventTreeItem *newParent = new EventTreeItem(event);
                newParent->appendChild(new EventTreeItem(event, newParent));
                eventRootItem->prependChild(newParent);

                emit q->endInsertRows();
            }
            break;
        }
        case CallModel::SortByTime:
        {
            // reset event count if type doesn't match top event
            if (!eventMatchesFilter(event) && eventRootItem->childCount()) {
                EventTreeItem *topItem = eventRootItem->child(0);
                if (event.recipients().matches(topItem->event().recipients())) {
                    EventTreeItem *newTopItem = new EventTreeItem(topItem->event());
                    newTopItem->event().setEventCount(1);

                    eventRootItem->removeAt(0);
                    eventRootItem->prependChild(newTopItem);

                    emitDataChanged(0, newTopItem);
                    return;
                }
            }

            if (!eventMatchesFilter(event))
                return;

            // if new item is groupable with the first one in the list
            // NOTE: assumption is that time value is ok
            if (eventRootItem->childCount()
                && eventRootItem->child(0)->event().eventCount() != -1
                && belongToSameGroup(event, eventRootItem->child(0)->event())) {
                // alias
                EventTreeItem *firstTopLevelItem = eventRootItem->child(0);
                // add event to the group, set it as top level item and refresh event count
                firstTopLevelItem->prependChild(new EventTreeItem(event, firstTopLevelItem));
                firstTopLevelItem->setEvent(event);
                firstTopLevelItem->event().setEventCount(calculateEventCount(firstTopLevelItem));
                // only counter and timestamp of first must be updated
                emitDataChanged(0, firstTopLevelItem);
            } else { // create a new group, otherwise
                // a new row must be inserted
                q->beginInsertRows(QModelIndex(), 0, 0);
                // add new item as first on the list
                eventRootItem->prependChild(new EventTreeItem(event));
                // alias
                EventTreeItem *firstTopLevelItem = eventRootItem->child(0);
                // add the copy of the event to its local list and refresh event count
                firstTopLevelItem->prependChild(new EventTreeItem(event, firstTopLevelItem));
                firstTopLevelItem->event().setEventCount(calculateEventCount(firstTopLevelItem));
                q->endInsertRows();
            }
            break;
        }
        default:
        {
            qCWarning(lcCommHistory) << Q_FUNC_INFO << "Adding call events to model sorted by type or by service has not been implemented yet.";
            return;
        }
    }
}

void CallModelPrivate::eventsAddedSlot(const QList<Event> &events)
{
    qCDebug(lcCommHistory) << Q_FUNC_INFO << events.count();
    // TODO: sorting?
    EventModelPrivate::eventsAddedSlot(events);
}

void CallModelPrivate::eventsUpdatedSlot(const QList<Event> &events)
{
    Q_Q(CallModel);

    // TODO regrouping of events might occur =(

    // reimp from EventModelPrivate, plus additional isVideoCall processing
    QList<Event> additions;
    foreach (const Event &event, events) {
        qCDebug(lcCommHistory) << Q_FUNC_INFO << "updated" << event.toString();
        QModelIndex index = findEvent(event.id());
        Event e = event;

        if (!index.isValid()) {
            if (acceptsEvent(e))
                additions.append(e);

            continue;
        }

        EventTreeItem *item = static_cast<EventTreeItem *>(index.internalPointer());
        if (item) {
            Event oldEvent = item->event();
            if (oldEvent.isVideoCall() != event.isVideoCall()) {
                // Video call status up/downgraded; refetch both video-
                // and non-video-versions for the call group and process
                // the results in eventsReceived
                updatedGroups.insert(DatabaseIOPrivate::makeCallGroupURI(oldEvent));
                updatedGroups.insert(DatabaseIOPrivate::makeCallGroupURI(event));
            } else {
                modifyInModel(e);
            }
        }
    }

    if (!additions.isEmpty())
        addToModel(additions);

    qCDebug(lcCommHistory) << Q_FUNC_INFO << "updatedGroups" << updatedGroups;

    if (!updatedGroups.isEmpty()) {
        /*
         * *** TODO ***
         * Optimizing this would require a lot of tweaking to handle
         * split/merged/added/deleted rows. No time to do this right
         * now, so just force a refetch.
         */
        if (hasBeenFetched) {
            q->getEvents();
            return;
        }
    }
}

QModelIndex CallModelPrivate::findEvent(int id) const
{
    Q_Q(const CallModel);

    if (!isInTreeMode) {
        return EventModelPrivate::findEvent(id);
    }

    for (int row = 0; row < eventRootItem->childCount(); row++) {
        // check top level item
        if (eventRootItem->child(row)->event().id() == id) {
            return q->createIndex(row, 0, eventRootItem->child(row));
        }
        // loop through all grouped events
        EventTreeItem *currentGroup = eventRootItem->child(row);
        for (int column = 0; column < currentGroup->childCount(); column++) {
            if (currentGroup->child(column)->event().id() == id) {
                return q->createIndex(row, column + 1, currentGroup->child(column));
            }
        }
    }

    // id was not found, return invalid index
    return QModelIndex();
}

void CallModelPrivate::deleteFromModel(int id)
{
    Q_Q(CallModel);

    if (!isInTreeMode) {
        return EventModelPrivate::deleteFromModel(id);
    }

    // TODO : what if an event is deleted from the db through commhistory-tool?

    // seek for the top level item which was deleted
    QModelIndex index = findEvent(id);

    // if id was not found, do nothing
    if (!index.isValid()) {
        qCDebug(lcCommHistory) << Q_FUNC_INFO << "*** Invalid";
        return;
    }

    // if event is a top level item (i.e. the whole group), then delete it
    int row = index.row();
    int column = index.column();
    if (column == 0) {
        bool isRegroupingNeeded = false;
        // regrouping is needed/possible only if sorting is SortByTime...
        // ...and there is a previous row and a following row to group together
        if (sortBy == CallModel::SortByTime &&
             row - 1 >= 0 && row + 1 < eventRootItem->childCount()) {
            EventTreeItem *prev = eventRootItem->child(row - 1);
            EventTreeItem *next = eventRootItem->child(row + 1);

            if (belongToSameGroup(prev->event(), next->event())) {
                for (int i = 0; i < next->childCount(); i++) {
                    prev->appendChild(new EventTreeItem(next->child(i)->event()));
                }
                prev->event().setEventCount(calculateEventCount(prev));
                isRegroupingNeeded = true;
            }
        }

        qCDebug(lcCommHistory) << Q_FUNC_INFO << "*** Top level" << row;
        // if there is no need to regroup the previous and following items,
        // then delete only one row
        if (!isRegroupingNeeded) {
            q->beginRemoveRows(index.parent(), row, row);
            eventRootItem->removeAt(row);
        }
        // otherwise delete the current and the following one
        // (since we added content of the following to the previous)
        else {
            q->beginRemoveRows(index.parent(), row, row + 1);
            eventRootItem->removeAt(row + 1);
            eventRootItem->removeAt(row);
            emitDataChanged(row, eventRootItem->child(row - 1));
        }
        q->endRemoveRows();
    }
    // otherwise item is a grouped event
    else {
        EventTreeItem *group = eventRootItem->child(row);
        group->removeAt(column - 1);

        if (group->childCount() == 0) {
            q->beginRemoveRows(index.parent(), row, row);
            eventRootItem->removeAt(row);
            q->endRemoveRows();
        } else {
            // Update the group if necessary
            const int originalCount(group->event().eventCount());
            if (originalCount > 1) {
                // This is an approximation...
                group->event().setEventCount(originalCount - 1);
            }
            if (column == 1) {
                // Replace the top-level event with the next
                group->setEvent(group->child(0)->event());
            }

            if (originalCount > 1 || column == 1) {
                // This group has changed properties - see if the sorting has changed
                if (eventRootItem->childCount() > (row + 1)) {
                    const quint32 groupTime(group->event().endTimeT());
                    int newRow = row + 1;
                    if (eventRootItem->child(newRow)->event().endTimeT() > groupTime) {
                        // This group has to move
                        while ((newRow + 1) < eventRootItem->childCount()) {
                            if (eventRootItem->child(newRow + 1)->event().endTimeT() > groupTime) {
                                ++newRow;
                            }
                        }

                        q->beginMoveRows(QModelIndex(), row, row, QModelIndex(), newRow + 1);
                        eventRootItem->moveChild(row, newRow);
                        q->endMoveRows();

                        // update row data
                        emitDataChanged(newRow, group);
                        return;
                    }
                }

                // No move required, just emit dataChanged
                emitDataChanged(row, eventRootItem->child(row));
            }
        }
    }
}

void CallModelPrivate::slotAllCallsDeleted(int unused)
{
    Q_UNUSED(unused);
    Q_Q(CallModel);

    qCDebug(lcCommHistory) << Q_FUNC_INFO << "clearing model";

    q->beginResetModel();
    clearEvents();
    q->endResetModel();
}

void CallModelPrivate::recipientsUpdated(const QSet<Recipient> &recipients, bool resolved)
{
    Q_Q(CallModel);

    QList<int> changedIds;
    QList<int> removedIds;
    QList<Event> removedEvents;

    for (int row = 0; row < eventRootItem->childCount(); ++row) {
        EventTreeItem *child = eventRootItem->child(row);
        Event &event(child->event());
        if (event.recipients().intersects(recipients)) {
            if (resolved) {
                if (!event.isResolved() && event.recipients().allContactsResolved()) {
                    event.setIsResolved(true);

                    // Update the child events
                    for (int column = 0; column < child->childCount(); ++column) {
                        Event &subEvent(child->child(column)->event());
                        if (!subEvent.isResolved() && subEvent.recipients().allContactsResolved())
                            subEvent.setIsResolved(true);
                    }
                }
            }

            if (removedIds.contains(event.id()))
                continue;

            bool removed(false);
            if (sortBy != CallModel::SortByTime) {
                // Has the grouping been changed?
                for (int otherRow = 0; otherRow < eventRootItem->childCount(); ++otherRow) {
                    if (otherRow != row) {
                        const Event &otherEvent(eventRootItem->child(otherRow)->event());
                        if (belongToSameGroup(event, otherEvent)) {
                            // These events should be coalesced
                            removedEvents.append(otherRow < row ? event : otherEvent);
                            removedIds.append(removedEvents.last().id());
                            removed = true;
                            break;
                        }
                    }
                }
            }

            if (!removed)
                changedIds.append(event.id());
        }
    }

    if (!removedEvents.isEmpty()) {
        // Remove these events from their current groups
        foreach (const Event &event, removedEvents)
            deleteFromModel(event.id());

        // Reinsert into matching groups
        foreach (const Event &event, removedEvents) {
            int matchingRow = -1;
            int positionRow = -1;
            for (int i = 0; i < eventRootItem->childCount(); i++) {
                const Event &groupEvent(eventRootItem->child(i)->event());
                if (belongToSameGroup(groupEvent, event)) {
                    matchingRow = i;
                    break;
                } else if (groupEvent.endTimeT() > event.endTimeT()) {
                    positionRow = i + 1;
                }
            }

            if (matchingRow == -1) {
                // No match found
                emit q->beginInsertRows(QModelIndex(), positionRow, positionRow);

                EventTreeItem *newParent = new EventTreeItem(event);
                newParent->appendChild(new EventTreeItem(event, newParent));
                newParent->event().setEventCount(1);
                eventRootItem->insertChildAt(positionRow, newParent);

                emit q->endInsertRows();
            } else {
                EventTreeItem *matchingItem = eventRootItem->child(matchingRow);

                const int groupEventCount(matchingItem->event().eventCount());
                const bool increaseEventCount(matchingItem->event().direction() == event.direction() &&
                                              matchingItem->event().incomingStatus() == event.incomingStatus());

                int newChildIndex = 0;
                for (; newChildIndex < matchingItem->childCount(); ++newChildIndex) {
                    if (event.endTimeT() > matchingItem->eventAt(newChildIndex).endTimeT())
                        break;
                }

                matchingItem->insertChildAt(newChildIndex, new EventTreeItem(event, matchingItem));
                if (newChildIndex == 0)
                    matchingItem->setEvent(event);
                matchingItem->event().setEventCount(increaseEventCount ? groupEventCount + 1 : 1);

                int updatedGroupIndex = matchingRow;
                if (matchingRow > 0 && newChildIndex == 0) {
                    // The insertion of this event may change the top-level ordering
                    if (event.endTimeT() > eventRootItem->child(matchingRow - 1)->event().endTimeT()) {
                        for (updatedGroupIndex = 0; updatedGroupIndex < matchingRow; ++updatedGroupIndex) {
                            if (event.endTimeT() > eventRootItem->child(updatedGroupIndex)->event().endTimeT())
                                break;
                        }
                    }
                }

                if (updatedGroupIndex < matchingRow) {
                    // The group must be moved
                    q->beginMoveRows(QModelIndex(), matchingRow, matchingRow, QModelIndex(), updatedGroupIndex);
                    eventRootItem->moveChild(matchingRow, updatedGroupIndex);
                    q->endMoveRows();
                }

                // We must report the change to the group
                changedIds.append(matchingItem->event().id());
            }
        }
    }

    if (!changedIds.isEmpty()) {
        for (int row = 0; row < eventRootItem->childCount(); ++row) {
            EventTreeItem *child = eventRootItem->child(row);
            if (changedIds.contains(child->event().id()))
                emitDataChanged(row, child);
        }
    }
}

/* ************************************************************************** *
 * ********* P U B L I C   C L A S S   I M P L E M E N T A T I O N ********** *
 * ************************************************************************** */

CallModel::CallModel(QObject *parent)
        : EventModel(*new CallModelPrivate(this), parent)
{
    Q_D(CallModel);
    d->isInTreeMode = true;
}

CallModel::~CallModel()
{
}

void CallModel::setQueryMode(EventModel::QueryMode mode)
{
    EventModel::setQueryMode(mode);
}

bool CallModel::setFilter(CallModel::Sorting sortBy,
                          CallEvent::CallType type,
                          const QDateTime &referenceTime)
{
    Q_D(CallModel);

    setSorting(sortBy);
    setFilterType(type);
    setFilterReferenceTime(referenceTime);
    setFilterAccount(QString());

    if (d->hasBeenFetched) {
        return getEvents();
    }
    return true;
}

void CallModel::setSorting(CallModel::Sorting sortBy)
{
    Q_D(CallModel);
    d->sortBy = sortBy;
}

void CallModel::setFilterType(CallEvent::CallType type)
{
    Q_D(CallModel);
    d->eventType = type;
}

void CallModel::setFilterReferenceTime(const QDateTime &referenceTime)
{
    Q_D(CallModel);
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    d->referenceTime = referenceTime.isNull() ? 0 : referenceTime.toSecsSinceEpoch();
#else
    d->referenceTime = referenceTime.isNull() ? 0 : referenceTime.toTime_t();
#endif
}

void CallModel::setFilterAccount(const QString &localUid)
{
    Q_D(CallModel);
    d->filterLocalUid = localUid;
}

void CallModel::resetFilters()
{
    Q_D(CallModel);
    d->sortBy = SortByContact;
    d->eventType = CallEvent::UnknownCallType;
    d->referenceTime = 0;
    d->filterLocalUid = QString();
}

bool CallModel::getEvents()
{
    Q_D(CallModel);

    d->hasBeenFetched = true;

    beginResetModel();
    d->clearEvents();
    endResetModel();
    d->countedUids.clear();
    d->updatedGroups.clear();

    QString q = DatabaseIOPrivate::eventQueryBase();
    q += QString::fromLatin1("WHERE type=%1 ").arg(Event::CallEvent);

    if (d->eventType == CallEvent::ReceivedCallType) {
        q += QString::fromLatin1("AND direction=%1 AND isMissedCall=0 ").arg(Event::Inbound);
    } else if (d->eventType == CallEvent::MissedCallType) {
        q += QString::fromLatin1("AND direction=%1 AND isMissedCall=1 ").arg(Event::Inbound);
    } else if (d->eventType == CallEvent::DialedCallType) {
        q += QString::fromLatin1("AND direction=%1 ").arg(Event::Outbound);
    }

    if (!d->filterLocalUid.isEmpty()) {
        q += QString::fromLatin1("AND localUid=:filterLocalUid ");
    }

    if (d->referenceTime != 0) {
        q += QString::fromLatin1("AND startTime >= %1 ").arg(d->referenceTime);
    }

    q += "ORDER BY endTime DESC, id DESC";

    QSqlQuery query = d->prepareQuery(q);

    if (!d->filterLocalUid.isEmpty())
        query.bindValue(":filterLocalUid", d->filterLocalUid);

    return d->executeQuery(query);
}

bool CallModel::getEvents(CallModel::Sorting sortBy,
                          CallEvent::CallType type,
                          const QDateTime &referenceTime)
{
    Q_D(CallModel);

    d->hasBeenFetched = true;

    return setFilter(sortBy, type, referenceTime);
}

bool CallModel::deleteAll()
{
    Q_D(CallModel);

    bool deleted;
    deleted = d->database()->deleteAllEvents(Event::CallEvent);
    if (!deleted) {
        qCWarning(lcCommHistory) << Q_FUNC_INFO << "Failed to delete events";
        return false;
    }

    d->slotAllCallsDeleted(-1);
    emit d->eventsCommitted(QList<Event>(), true);
    return true;
}

bool CallModel::markAllRead()
{
    Q_D(CallModel);

    bool marked;
    marked = d->database()->markAsReadAll(Event::CallEvent);
    if (!marked) {
        qCWarning(lcCommHistory) << Q_FUNC_INFO << "Failed to mark events as read";
        return false;
    }

    emit d->eventsCommitted(QList<Event>(), true);
    return true;
}

bool CallModel::addEvent(Event &event)
{
    return EventModel::addEvent(event);
}

bool CallModel::modifyEvent(Event &event)
{
    Q_D(CallModel);

    if (!d->isInTreeMode || !event.modifiedProperties().contains(Event::IsRead)) {
        return EventModel::modifyEvent(event);
    }

    if (event.id() == -1) {
        qCWarning(lcCommHistory) << Q_FUNC_INFO << "Event id not set";
        return false;
    }

    qCDebug(lcCommHistory) << Q_FUNC_INFO << "setting isRead for call group";
    // isRead has changed, modify the event and set isRead for nested events
    bool isRead = event.isRead();

    QList<Event> events;
    events << event;

    QModelIndex index = d->findEvent(event.id());
    if (index.isValid()) {
        EventTreeItem *item = static_cast<EventTreeItem *>(index.internalPointer());
        if (item) {
            // child 0 = event
            for (int i = 1; i < item->childCount(); i++) {
                Event &e = item->child(i)->event();
                if (e.isRead() != isRead) {
                    e.setIsRead(isRead);
                    events << e;
                }
            }
        }
    }

    return EventModel::modifyEvents(events);
}

bool CallModel::deleteEvent(int id)
{
    Q_D(CallModel);

    if (!d->isInTreeMode) {
        return EventModel::deleteEvent(id);
    }

    qCDebug(lcCommHistory) << Q_FUNC_INFO << id;
    QModelIndex index = d->findEvent(id);
    if (!index.isValid())
        return false;

    switch (d->sortBy)
    {
        case SortByContact:
        case SortByContactAndType:
        case SortByTime:
        {
            EventTreeItem *item = d->eventRootItem->child(index.row());

            if (!d->database()->transaction())
                return false;

            QList<Event> deletedEvents;

            // get all events stored in the item and delete them one by one
            for (int i = 0; i < item->childCount(); i++) {
                // NOTE: when events are sorted by time, the tree hierarchy is only 2 levels deep
                if (!d->database()->deleteEvent(item->child(i)->event())) {
                    d->database()->rollback();
                    return false;
                }
                deletedEvents << item->child(i)->event();
            }

            if (!d->database()->commit())
                return false;

            // delete event from model (not only from db)
            d->deleteFromModel(id);
            // signal delete in case someone else needs to know it
            foreach (const Event &e, deletedEvents)
                emit d->eventDeleted(e.id());
            emit d->eventsCommitted(deletedEvents, true);

            return true;
        }
        default:
        {
            qCWarning(lcCommHistory) << Q_FUNC_INFO << "Deleting of call events from model sorted by type or by service has not been implemented yet.";
            return false;
        }
    }
}

bool CallModel::deleteEvent(Event &event)
{
    Q_D(CallModel);
    if (!d->isInTreeMode)
        return EventModel::deleteEvent(event);
    return deleteEvent(event.id());
}

}

#include "callmodel.moc"

