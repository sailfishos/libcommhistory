/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013-2017 Jolla Ltd. <john.brooks@jollamobile.com>
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef COMMHISTORY_DATABASEIO_H
#define COMMHISTORY_DATABASEIO_H

#include <QObject>
#include <QUrl>

#include "event.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class DatabaseIOPrivate;
class Group;

/**
 * \class DatabaseIO
 *
 * Class for handling events with the database. You can use this if you are
 * implementing your own model.
 */
class LIBCOMMHISTORY_EXPORT DatabaseIO : public QObject
{
    Q_OBJECT

public:
    DatabaseIO();
    ~DatabaseIO();
    static DatabaseIO* instance();

    /*!
     * Add a new event into the database. The id field of the event is
     * updated if successfully added.
     *
     * \param event New event.
     * \return true if successful, otherwise false
     */
    bool addEvent(Event &event);

    /*!
     * Reserves a sequence of \a count event id(s) starting from \a firstReservedId. Main use case
     * is reservation of ids for events which are only stored in model and not saved in database.
     *
     * \param count Number of ids to reserve.
     * \param firstReservedId First reserved event id.
     * \return true if successful, otherwise false
     */
    bool reserveEventIds(int count, int *firstReservedId);

    /*!
     * Add a new group into the database. The id field of the group is
     * updated if successfully added.
     *
     * \param group New group.
     * \return true if successful, otherwise false
     */
    bool addGroup(Group &group);

    /*!
     * Query a single event by id.
     *
     * \param id Database id of the event.
     * \param event Return value for event details.
     * \return true if successful, otherwise false
     */
    bool getEvent(int id, Event &event);

    /*!
     * Query a single event by message token.
     *
     * \param token Message token
     * \param event Return value for event details.
     * \return true if successful, otherwise false
     */
    bool getEventByMessageToken(const QString &token, Event &event);

    /*!
     * Query a single inbound MMS event by mms id.
     *
     * \param mmsId mms id
     * \param event Return value for event details.
     * \return true if successful, otherwise false
     */
    bool getEventByMmsId(const QString &mmsId, Event &event);

    /*!
     * Checks if event with the specified id exists in the database.
     *
     * \param id event id
     * \return true if event exists, otherwise false
     */
    bool eventExists(int id);

    /*!
     * Get extra property fields for an event.
     *
     * \param event Event instance to query and update with extra properties
     * \return true if successful, otherwise false
     */
    bool getEventExtraProperties(Event &event);

    /*!
     * Get message parts related to an event.
     *
     * \param event Event instance to query and update
     * \return true if successful, otherwise false
     */
    bool getMessageParts(Event &event);

    /*!
     * Modifye an event.
     *
     * \param event Existing event.
     * \return true if successful, otherwise false
     */
    bool modifyEvent(Event &event);

    /*!
     * Move an event to a new group
     *
     * \param event Existing event
     * \param groupId new group id
     *
     * \return true if successful, otherwise false
     */
    bool moveEvent(Event &event, int groupId);

    /*!
     * Delete an event
     *
     * \param event Existing event to delete
     * \param backgroundThread optional thread (to delete mms attachments)
     *
     * \return true if successful, otherwise false
     */
    bool deleteEvent(Event &event, QThread *backgroundThread = 0);

    /*!
     * Query a single group by id.
     *
     * \param id Database id of the group.
     * \param group Return value for group details.
     * \return true if successful, otherwise false
     */
    bool getGroup(int id, Group &group);

    /*!
     * Query groups, optionally by local or remote UID
     *
     * \param localUid Optional local UID to limit results
     * \param remoteUid Optional remote UID to limit results
     * \param groups Reference to container for results
     * \return true if successful, otherwise false
     */
    bool getGroups(const QString &localUid, const QString &remoteUid, QList<Group> &groups,
                   const QString &queryOrder = QString());

    /*!
     * Modifye a group.
     *
     * \param event Existing group.
     * \return true if successful, otherwise false
     */
    bool modifyGroup(Group &group);

    /*!
     * Delete a group
     *
     * \param groupId Existing group id
     * \param backgroundThread optional thread (to delete mms attachments)
     *
     * \return true if successful, otherwise false
     */
    bool deleteGroup(int groupId, QThread *backgroundThread = 0);

    /*!
     * Delete groups
     *
     * \param groupIds Existing group ids
     * \param backgroundThread optional thread (to delete mms attachments)
     *
     * \return true if successful, otherwise false
     */
    bool deleteGroups(QList<int> groupIds, QThread *backgroundThread = 0);

    /*!
     * Query the number of events in a group
     *
     * \param groupId Existing group id
     * \param totalEvents result
     *
     * \return true if successful, otherwise false
     */
    bool totalEventsInGroup(int groupId, int &totalEvents);

    /*!
     * Mark all messages in a group as read
     *
     * \param groupId Existing group id
     *
     * \return true if successful, otherwise false
     */
    bool markAsReadGroup(int groupId);

    /*!
     * Mark messages as read
     *
     * \param eventIds list of events to mark
     *
     * \return true if successful, otherwise false
     */
    bool markAsRead(const QList<int> &eventIds);

    /*!
     * Mark all messages of a certain type as read
     *
     * \param eventType
     *
     * \return true if successful. Sets lastError() on failure.
     */
    bool markAsReadAll(Event::EventType eventType);

    /*!
     * Delete events of a certain type
     *
     * If Event::UnknownType is passed, all events are deleted.
     *
     * \param eventType
     * \return true if successful, otherwise false
     */
    bool deleteAllEvents(Event::EventType eventType);

    /*!
     * Initate a new database transaction.
     */
    bool transaction();

    /*!
     * Commits the current transaction.
     *
     * \param isBlocking if true, the call blocks until changes are saved
     *                   if false, the call is asynchronous and returns immediately
     * \return transaction object to track commit progress for non-blocking call
     */
    bool commit();

    /*!
     * Cancels the current transaction.
     */
    bool rollback();

private:
    friend class DatabaseIOPrivate;
    DatabaseIOPrivate * const d;
};

} // namespace

#endif
