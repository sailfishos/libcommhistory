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

#include "groupmodel.h"
#include "groupmodel_p.h"
#include "eventmodel.h"
#include "group.h"
#include "groupmanager.h"
#include "groupobject.h"
#include "debug_p.h"

using namespace CommHistory;

namespace {

bool initializeTypes()
{
    qRegisterMetaType<QList<CommHistory::Event> >();
    qRegisterMetaType<QList<CommHistory::Group> >();
    qRegisterMetaType<QList<int> >();
    return true;
}

inline bool groupObjectSort(GroupObject *a, GroupObject *b)
{
    return a->endTimeT() > b->endTimeT(); // descending order
}

}

bool groupmodel_initialized = initializeTypes();

GroupModelPrivate::GroupModelPrivate(GroupModel *model)
        : q_ptr(model)
        , manager(0)
{
}

GroupModelPrivate::~GroupModelPrivate()
{
}

void GroupModelPrivate::ensureManager()
{
    if (!manager)
        setManager(new GroupManager(this));
}

void GroupModelPrivate::setManager(GroupManager *m)
{
    Q_Q(GroupModel);

    if (m == manager)
        return;

    q->beginResetModel();
    groups.clear();

    if (manager) {
        disconnect(manager, 0, this, 0);
        disconnect(manager, 0, q, 0);
    }

    manager = m;

    if (manager) {
        connect(manager, SIGNAL(groupAdded(GroupObject*)), SLOT(groupAdded(GroupObject*)));
        connect(manager, SIGNAL(groupUpdated(GroupObject*)), SLOT(groupUpdated(GroupObject*)));
        connect(manager, SIGNAL(groupDeleted(GroupObject*)), SLOT(groupDeleted(GroupObject*)));

        connect(manager, SIGNAL(modelReady(bool)), q, SIGNAL(modelReady(bool)));
        connect(manager, SIGNAL(groupsCommitted(QList<int>,bool)), q, SIGNAL(groupsCommitted(QList<int>,bool)));

        groups = manager->groups();
        std::sort(groups.begin(), groups.end(), groupObjectSort);
    }

    q->endResetModel();
    if (manager && manager->isReady())
        emit q->modelReady(true);
}

void GroupModelPrivate::groupAdded(GroupObject *group)
{
    Q_Q(GroupModel);

    int index = 0;
    for (; index < groups.size(); index++) {
        if (groupObjectSort(group, groups[index]))
            break;
    }

    q->beginInsertRows(QModelIndex(), index, index);
    groups.insert(index, group);
    q->endInsertRows();
}

void GroupModelPrivate::groupUpdated(GroupObject *group)
{
    Q_Q(GroupModel);

    int index = groups.indexOf(group);
    if (index < 0)
        return;

    int newIndex = index;
    for (int i = index - 1; i >= 0; i--) {
        if (groupObjectSort(group, groups[i]))
            newIndex = i;
        else
            break;
    }

    for (int i = index + 1; i < groups.size(); i++) {
        if (groupObjectSort(groups[i], group))
            newIndex = i;
        else
            break;
    }

    qCDebug(lcCommHistory) << Q_FUNC_INFO << index << newIndex;

    if (newIndex != index) {
        q->beginMoveRows(QModelIndex(), index, index, QModelIndex(), newIndex > index ? newIndex + 1 : newIndex);
        qCDebug(lcCommHistory) << Q_FUNC_INFO << "move" << index << newIndex;
        groups.move(index, newIndex);
        q->endMoveRows();
    }

    emit q->dataChanged(q->index(newIndex), q->index(newIndex));
}

void GroupModelPrivate::groupDeleted(GroupObject *group)
{
    Q_Q(GroupModel);

    int index = groups.indexOf(group);
    if (index < 0)
        return;

    q->beginRemoveRows(QModelIndex(), index, index);
    groups.removeAt(index);
    q->endRemoveRows();
}

GroupModel::GroupModel(QObject *parent)
    : QAbstractListModel(parent),
      d(new GroupModelPrivate(this))
{
}

QHash<int, QByteArray> GroupModel::roleNames() const
{
    QHash<int,QByteArray> roles;
    roles[GroupIdRole] = "groupId";
    roles[LocalUidRole] = "localUid";
    roles[RemoteUidsRole] = "remoteUids";
    roles[ChatNameRole] = "chatName";
    roles[EndTimeRole] = "endTime";
    roles[UnreadMessagesRole] = "unreadMessages";
    roles[LastEventIdRole] = "lastEventId";
    roles[ContactsRole] = "contacts";
    roles[LastMessageTextRole] = "lastMessageText";
    roles[LastVCardFileNameRole] = "lastVCardFileName";
    roles[LastVCardLabelRole] = "lastVCardLabel";
    roles[LastEventTypeRole] = "lastEventType";
    roles[LastEventStatusRole] = "lastEventStatus";
    roles[LastModifiedRole] = "lastModified";
    roles[StartTimeRole] = "startTime";
    roles[GroupRole] = "group";
    roles[ContactIdsRole] = "contactIds";
    roles[GroupObjectRole] = "group";
    roles[TimeSectionRole] = "timeSection";
    return roles;
}

GroupModel::~GroupModel()
{
    delete d;
    d = 0;
}

GroupManager *GroupModel::manager() const
{
    return d->manager;
}

void GroupModel::setManager(GroupManager *m)
{
    if (m == d->manager)
        return;

    d->setManager(m);
}

void GroupModel::setQueryMode(EventModel::QueryMode mode)
{
    d->ensureManager();
    d->manager->setQueryMode(mode);
}

void GroupModel::setChunkSize(uint size)
{
    d->ensureManager();
    d->manager->setChunkSize(size);
}

void GroupModel::setFirstChunkSize(uint size)
{
    d->ensureManager();
    d->manager->setFirstChunkSize(size);
}

void GroupModel::setLimit(int limit)
{
    d->ensureManager();
    d->manager->setLimit(limit);
}

void GroupModel::setOffset(int offset)
{
    d->ensureManager();
    d->manager->setOffset(offset);
}

int GroupModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return d->groups.count();
}

QVariant GroupModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= d->groups.count()) {
        return QVariant();
    }

    GroupObject *group = d->groups.value(index.row());
    if (!group)
        return QVariant();

    switch (role) {
    case GroupRole:
        return QVariant::fromValue(group->toGroup());
    case GroupObjectRole:
        return QVariant::fromValue<QObject*>(group);
    case ContactIdsRole:
        return QVariant::fromValue<QList<int> >(group->recipients().contactIds());
    case TimeSectionRole:
        return group->endTime().toLocalTime().date();
    case GroupIdRole:
        return QVariant::fromValue(group->id());
    case LocalUidRole:
        return QVariant::fromValue(group->localUid());
    case RemoteUidsRole:
        return QVariant::fromValue(group->recipients().remoteUids());
    case ChatNameRole:
        return QVariant::fromValue(group->chatName());
    case EndTimeRole:
        return QVariant::fromValue(group->endTime());
    case UnreadMessagesRole:
        return QVariant::fromValue(group->unreadMessages());
    case LastEventIdRole:
        return QVariant::fromValue(group->lastEventId());
    case ContactsRole:
        return QVariant::fromValue(group->recipients().contactIds());
    case LastMessageTextRole:
        return QVariant::fromValue(group->lastMessageText());
    case LastVCardFileNameRole:
        return QVariant::fromValue(group->lastVCardFileName());
    case LastVCardLabelRole:
        return QVariant::fromValue(group->lastVCardLabel());
    case LastEventTypeRole:
        return QVariant::fromValue((int) group->lastEventType());
    case LastEventStatusRole:
        return QVariant::fromValue((int) group->lastEventStatus());
    case LastModifiedRole:
        return QVariant::fromValue(group->lastModified());
    case StartTimeRole:
        return QVariant::fromValue(group->startTime());
    default:
        qCWarning(lcCommHistory) << "Group::data: invalid role??" << role;
        return QVariant();
    }
}

Group GroupModel::group(const QModelIndex &index) const
{
    GroupObject *go = d->groups.value(index.row());
    if (go)
        return go->toGroup();
    return Group();
}

GroupObject *GroupModel::groupObject(const QModelIndex &index) const
{
    return d->groups.value(index.row());
}

QModelIndex GroupModel::findGroup(int id) const
{
    for (int i = 0; i < d->groups.size(); ++i) {
        if (d->groups[i]->id() == id)
            return index(i, 0, QModelIndex());
    }
    return QModelIndex();
}

bool GroupModel::addGroup(Group &group)
{
    d->ensureManager();
    return d->manager->addGroup(group);
}

bool GroupModel::addGroups(QList<Group> &groups)
{
    d->ensureManager();
    return d->manager->addGroups(groups);
}

bool GroupModel::modifyGroup(Group &group)
{
    d->ensureManager();
    return d->manager->modifyGroup(group);
}

bool GroupModel::getGroups(const QString &localUid,
                           const QString &remoteUid)
{
    d->ensureManager();
    return d->manager->getGroups(localUid, remoteUid);
}

bool GroupModel::markAsReadGroup(int id)
{
    d->ensureManager();
    return d->manager->markAsReadGroup(id);
}

void GroupModel::updateGroups(QList<Group> &groups)
{
    d->ensureManager();
    return d->manager->updateGroups(groups);
}

bool GroupModel::deleteGroups(const QList<int> &groupIds)
{
    d->ensureManager();
    return d->manager->deleteGroups(groupIds);
}

bool GroupModel::deleteAll()
{
    d->ensureManager();
    return d->manager->deleteAll();
}

bool GroupModel::canFetchMore(const QModelIndex &parent) const
{
    if (parent.isValid() || !d->manager)
        return false;

    return d->manager->canFetchMore();
}

void GroupModel::fetchMore(const QModelIndex &parent)
{
    if (!parent.isValid() && d->manager)
        d->manager->fetchMore();
}

bool GroupModel::isReady() const
{
    return d->manager && d->manager->isReady();
}

EventModel::QueryMode GroupModel::queryMode() const
{
    d->ensureManager();
    return d->manager->queryMode();
}

uint GroupModel::chunkSize() const
{
    d->ensureManager();
    return d->manager->chunkSize();
}

uint GroupModel::firstChunkSize() const
{
    d->ensureManager();
    return d->manager->firstChunkSize();
}

int GroupModel::limit() const
{
    d->ensureManager();
    return d->manager->limit();
}

int GroupModel::offset() const
{
    d->ensureManager();
    return d->manager->offset();
}

void GroupModel::setBackgroundThread(QThread *thread)
{
    d->ensureManager();
    d->manager->setBackgroundThread(thread);
}

QThread* GroupModel::backgroundThread()
{
    d->ensureManager();
    return d->manager->backgroundThread();
}

DatabaseIO& GroupModel::databaseIO()
{
    d->ensureManager();
    return d->manager->databaseIO();
}

void GroupModel::setResolveContacts(GroupManager::ContactResolveType type)
{
    d->ensureManager();
    d->manager->setResolveContacts(type);
}

