/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd.
** Contact: John Brooks <john.brooks@jollamobile.com>
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

#include "contactgroup.h"
#include "updatesemitter.h"
#include "databaseio.h"
#include "debug_p.h"

namespace CommHistory {

class ContactGroupPrivate {
    Q_DECLARE_PUBLIC(ContactGroup)

public:
    ContactGroupPrivate(ContactGroup *parent);

    void updateForGroup(GroupObject *group,
                        quint32 &uStartTimeT, quint32 &uEndTimeT, quint32 &uLastModifiedT,
                        int &uUnreadMessages, QString &uSubscriberIdentity, GroupObject *&uLastEventGroup);
    void setValues(const QList<int> &uContactIds, const QStringList &uDisplayNames,
                   quint32 &uStartTimeT, quint32 &uEndTimeT, quint32 &uLastModifiedT,
                   int &uUnreadMessages, const QString &uSubscriberIdentity, GroupObject *&uLastEventGroup);

    void recalculate();
    void includeGroup(GroupObject *group);

    void resolve();

    ContactGroup *q_ptr;
    QList<GroupObject*> groups;
 
    QList<int> contactIds;
    QStringList displayNames;
    quint32 startTimeT, endTimeT, lastModifiedT;
    int unreadMessages;
    int lastEventId;
    GroupObject *lastEventGroup;
    QString lastMessageText, lastVCardFileName, lastVCardLabel, subscriberIdentity;
    int lastEventType, lastEventStatus;
    bool lastEventIsDraft;
    bool groupsResolved;
};

}

using namespace CommHistory;

ContactGroupPrivate::ContactGroupPrivate(ContactGroup *q)
    : q_ptr(q)
    , startTimeT(0)
    , endTimeT(0)
    , lastModifiedT(0)
    , unreadMessages(0)
    , lastEventId(-1)
    , lastEventGroup(0)
    , lastEventType(Event::UnknownType)
    , lastEventStatus(Event::UnknownStatus)
    , lastEventIsDraft(false)
    , groupsResolved(true)
{
}

ContactGroup::ContactGroup(QObject *parent)
    : QObject(parent), d_ptr(new ContactGroupPrivate(this))
{
}

void ContactGroup::addGroup(GroupObject *group)
{
    Q_D(ContactGroup);
    if (!d->groups.contains(group)) {
        d->groupsResolved &= group->isResolved();
        d->groups.append(group);
        emit groupsChanged();

        d->includeGroup(group);
    }
}

bool ContactGroup::removeGroup(GroupObject *group)
{
    Q_D(ContactGroup);
    if (d->groups.removeOne(group)) {
        emit groupsChanged();
        d->recalculate();
    }

    return d->groups.isEmpty();
}

void ContactGroup::updateGroup(GroupObject *group)
{
    Q_D(ContactGroup);

    Q_UNUSED(group);
    d->recalculate();
}

void ContactGroupPrivate::updateForGroup(GroupObject *group,
                                         quint32 &uStartTimeT, quint32 &uEndTimeT, quint32 &uLastModifiedT,
                                         int &uUnreadMessages, QString &uSubscriberIdentity, GroupObject *&uLastEventGroup)
{
    const quint32 gStartTimeT = group->startTimeT();
    const quint32 gEndTimeT = group->endTimeT();
    const quint32 gLastModifiedT = group->lastModifiedT();
    const QString gSubscriberIdentity = group->subscriberIdentity();

    uStartTimeT = qMax(gStartTimeT, uStartTimeT);
    uEndTimeT = qMax(gEndTimeT, uEndTimeT);
    uLastModifiedT = qMax(gLastModifiedT, uLastModifiedT);

    uUnreadMessages += group->unreadMessages();

    uSubscriberIdentity = uEndTimeT >= gEndTimeT ? uSubscriberIdentity : gSubscriberIdentity;

    if (group->lastEventId() >= 0 && (!uLastEventGroup || gEndTimeT > uLastEventGroup->endTimeT()))
        uLastEventGroup = group;
}

void ContactGroupPrivate::setValues(const QList<int> &uContactIds, const QStringList &uDisplayNames,
                                    quint32 &uStartTimeT, quint32 &uEndTimeT, quint32 &uLastModifiedT,
                                    int &uUnreadMessages, const QString &uSubscriberIdentity, GroupObject *&uLastEventGroup)
{
    Q_Q(ContactGroup);

    const bool contactsChanged(uContactIds != contactIds || uDisplayNames != displayNames);

    if (uContactIds != contactIds) {
        contactIds = uContactIds;
        emit q->contactIdsChanged();
    }

    if (uDisplayNames != displayNames) {
        displayNames = uDisplayNames;
        emit q->displayNamesChanged();
    }

    // Temporary: to be removed
    if (contactsChanged)
        emit q->contactsChanged();

    if (uStartTimeT != startTimeT) {
        startTimeT = uStartTimeT;
        emit q->startTimeChanged();
    }

    if (uEndTimeT != endTimeT) {
        endTimeT = uEndTimeT;
        emit q->endTimeChanged();
    }

    if (uLastModifiedT != lastModifiedT) {
        lastModifiedT = uLastModifiedT;
        emit q->lastModifiedChanged();
    }

    if (uUnreadMessages != unreadMessages) {
        unreadMessages = uUnreadMessages;
        emit q->unreadMessagesChanged();
    }

    if (uSubscriberIdentity != subscriberIdentity) {
        subscriberIdentity = uSubscriberIdentity;
        emit q->subscriberIdentityChanged();
    }

    if (uLastEventGroup) {
        bool changed = false;
        if (uLastEventGroup != lastEventGroup) {
            lastEventGroup = uLastEventGroup;
            changed = true;
        } else if (lastEventId != lastEventGroup->lastEventId() ||
                   lastMessageText != lastEventGroup->lastMessageText() ||
                   lastVCardFileName != lastEventGroup->lastVCardFileName() ||
                   lastVCardLabel != lastEventGroup->lastVCardLabel() ||
                   lastEventType != lastEventGroup->lastEventType() ||
                   lastEventStatus != lastEventGroup->lastEventStatus() ||
                   lastEventIsDraft != lastEventGroup->lastEventIsDraft()) {
            changed = true;
        }

        if (changed) {
            lastEventId = lastEventGroup->lastEventId();
            lastMessageText = lastEventGroup->lastMessageText();
            lastVCardFileName = lastEventGroup->lastVCardFileName();
            lastVCardLabel = lastEventGroup->lastVCardLabel();
            lastEventType = static_cast<int>(lastEventGroup->lastEventType());
            lastEventStatus = static_cast<int>(lastEventGroup->lastEventStatus());
            lastEventIsDraft = lastEventGroup->lastEventIsDraft();

            emit q->lastEventChanged();
        }
    } else if (lastEventId >= 0) {
        lastEventId = -1;
        lastMessageText.clear();
        lastVCardFileName.clear();
        lastVCardLabel.clear();
        lastEventType = static_cast<int>(Event::UnknownType);
        lastEventStatus = static_cast<int>(Event::UnknownStatus);
        lastEventIsDraft = false;

        emit q->lastEventChanged();
    }
}

void ContactGroupPrivate::recalculate()
{
    /* Iterate all groups to recalculate properties. Unfortunately, we can't rely on just comparing
       the changes to one group and have reliable data.
     */

    QList<int> uContactIds;
    QStringList uDisplayNames;
    quint32 uStartTimeT = 0, uEndTimeT = 0, uLastModifiedT = 0;
    int uUnreadMessages = 0;
    QString uSubscriberIdentity;
    GroupObject *uLastEventGroup = 0;

    if (!groups.isEmpty()) {
        /* Because of the mechanics of hasSameContacts, these values must be the
         * same for all groups, so we can just use the first. */
        uContactIds = groups[0]->recipients().contactIds();
        uDisplayNames = groups[0]->recipients().displayNames();

        /* Set the initial subscriberIdentity, to be updated to most recent event value. */
        uSubscriberIdentity = groups[0]->subscriberIdentity();
    }

    foreach (GroupObject *group, groups)
        updateForGroup(group, uStartTimeT, uEndTimeT, uLastModifiedT, uUnreadMessages, uSubscriberIdentity, uLastEventGroup);

    setValues(uContactIds, uDisplayNames, uStartTimeT, uEndTimeT, uLastModifiedT, uUnreadMessages, uSubscriberIdentity, uLastEventGroup);
}

void ContactGroupPrivate::includeGroup(GroupObject *group)
{
    const bool firstGroup(lastEventGroup == 0);

    // See if this group changes any collective properties
    QList<int> uContactIds(firstGroup ? group->recipients().contactIds() : contactIds);
    QStringList uDisplayNames(firstGroup ? group->recipients().displayNames() : displayNames);
    quint32 uStartTimeT = startTimeT, uEndTimeT = endTimeT, uLastModifiedT = lastModifiedT;
    int uUnreadMessages = unreadMessages;
    QString uSubscriberIdentity = group->subscriberIdentity();
    GroupObject *uLastEventGroup = lastEventGroup;

    updateForGroup(group, uStartTimeT, uEndTimeT, uLastModifiedT, uUnreadMessages, uSubscriberIdentity, uLastEventGroup);

    setValues(uContactIds, uDisplayNames, uStartTimeT, uEndTimeT, uLastModifiedT, uUnreadMessages, uSubscriberIdentity, uLastEventGroup);
}

QList<int> ContactGroup::contactIds() const
{
    Q_D(const ContactGroup);
    if (d->groups.isEmpty())
        return QList<int>();

    // We need our groups resolved to give (asynchronously) the correct result
    const_cast<ContactGroupPrivate *>(d)->resolve();
    return d->groups[0]->recipients().contactIds();
}

QStringList ContactGroup::displayNames() const
{
    Q_D(const ContactGroup);
    if (d->groups.isEmpty())
        return QStringList();

    // We need our groups resolved to give (asynchronously) the correct result
    const_cast<ContactGroupPrivate *>(d)->resolve();
    return d->groups[0]->recipients().displayNames();
}

QDateTime ContactGroup::startTime() const
{
    Q_D(const ContactGroup);
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    return QDateTime::fromSecsSinceEpoch(d->startTimeT);
#else
    return QDateTime::fromTime_t(d->startTimeT);
#endif
}

QDateTime ContactGroup::endTime() const
{
    Q_D(const ContactGroup);
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    return QDateTime::fromSecsSinceEpoch(d->endTimeT);
#else
    return QDateTime::fromTime_t(d->endTimeT);
#endif
}

int ContactGroup::unreadMessages() const
{
    Q_D(const ContactGroup);
    return d->unreadMessages;
}

int ContactGroup::lastEventId() const
{
    Q_D(const ContactGroup);
    return d->lastEventId;
}

GroupObject *ContactGroup::lastEventGroup() const
{
    Q_D(const ContactGroup);
    return d->lastEventGroup;
}

QString ContactGroup::lastMessageText() const
{
    Q_D(const ContactGroup);
    return d->lastMessageText;
}

QString ContactGroup::lastVCardFileName() const
{
    Q_D(const ContactGroup);
    return d->lastVCardFileName;
}

QString ContactGroup::lastVCardLabel() const
{
    Q_D(const ContactGroup);
    return d->lastVCardLabel;
}

int ContactGroup::lastEventType() const
{
    Q_D(const ContactGroup);
    return d->lastEventType;
}

int ContactGroup::lastEventStatus() const
{
    Q_D(const ContactGroup);
    return d->lastEventStatus;
}

bool ContactGroup::lastEventIsDraft() const
{
    Q_D(const ContactGroup);
    return d->lastEventIsDraft;
}

QDateTime ContactGroup::lastModified() const
{
    Q_D(const ContactGroup);
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    return QDateTime::fromSecsSinceEpoch(d->lastModifiedT);
#else
    return QDateTime::fromTime_t(d->lastModifiedT);
#endif
}

QString ContactGroup::subscriberIdentity() const
{
    Q_D(const ContactGroup);
    return d->subscriberIdentity;
}

QList<GroupObject*> ContactGroup::groups() const
{
    Q_D(const ContactGroup);
    return d->groups;
}

QList<QObject*> ContactGroup::groupObjects() const
{
    Q_D(const ContactGroup);
    QList<QObject*> l;
    l.reserve(d->groups.size());
    foreach (GroupObject *o, d->groups)
        l.append(o);
    return l;
}

bool ContactGroup::markAsRead() 
{
    Q_D(ContactGroup);

    qCDebug(lcCommHistory) << Q_FUNC_INFO;
    if (d->groups.isEmpty() || !unreadMessages())
        return true;

    DatabaseIO *database = DatabaseIO::instance();
    if (!database->transaction())
        return false;

    foreach (GroupObject *group, d->groups) {
        if (group->unreadMessages() && !database->markAsReadGroup(group->id())) {
            database->rollback();
            return false;
        }
    }

    if (!database->commit())
        return false;

    QList<Group> updated;
    foreach (GroupObject *group, d->groups) {
        if (group->unreadMessages()) {
            group->setUnreadMessages(0);
            updated.append(group->toGroup());
        }
    }

    emit UpdatesEmitter::instance()->groupsUpdatedFull(updated);
    return true;
}

bool ContactGroup::deleteGroups()
{
    Q_D(ContactGroup);

    QList<int> ids;
    ids.reserve(d->groups.size());
    foreach (GroupObject *group, d->groups)
        ids.append(group->id());

    if (ids.isEmpty())
        return true;

    DatabaseIO *database = DatabaseIO::instance();
    if (!database->transaction())
        return false;

    if (!database->deleteGroups(ids)) {
        database->rollback();
        return false;
    }

    if (!database->commit())
        return false;

    emit UpdatesEmitter::instance()->groupsDeleted(ids);
    return true;
}

quint32 ContactGroup::startTimeT() const
{
    Q_D(const ContactGroup);
    return d->startTimeT;
}

quint32 ContactGroup::endTimeT() const
{
    Q_D(const ContactGroup);
    return d->endTimeT;
}

quint32 ContactGroup::lastModifiedT() const
{
    Q_D(const ContactGroup);
    return d->lastModifiedT;
}

bool ContactGroup::isResolved() const
{
    Q_D(const ContactGroup);
    return d->groupsResolved;
}

void ContactGroup::resolve()
{
    Q_D(ContactGroup);
    d->resolve();
}

void ContactGroupPrivate::resolve()
{
    if (!groupsResolved) {
        groupsResolved = true;
        foreach (GroupObject *group, groups)
            group->resolve();
    }
}

GroupObject *ContactGroup::findGroup(const QString &localUid, const QString &remoteUid)
{
    return findGroup(localUid, QStringList() << remoteUid);
}

GroupObject *ContactGroup::findGroup(const QString &localUid, const QStringList &remoteUids)
{
    Q_D(ContactGroup);

    RecipientList match = RecipientList::fromUids(localUid, remoteUids);
    foreach (GroupObject *g, d->groups) {
        if (g->recipients().matches(match))
            return g;
    }

    return 0;
}

