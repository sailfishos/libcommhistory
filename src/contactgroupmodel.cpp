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

#include <QtDBus/QtDBus>
#include <QStringList>

#include "contactgroupmodel.h"
#include "groupmanager.h"
#include "contactgroup.h"
#include "debug_p.h"

using namespace CommHistory;

namespace {

bool initializeTypes()
{
    qRegisterMetaType<QList<CommHistory::GroupObject*> >();
    qRegisterMetaType<CommHistory::GroupObject*>();
    qRegisterMetaType<CommHistory::ContactGroup*>();
    return true;
}

inline bool contactGroupSort(ContactGroup *a, ContactGroup *b)
{
    return a->endTimeT() > b->endTimeT(); // descending order
}

}

bool contactgroupmodel_initialized = initializeTypes();

namespace CommHistory {

class GroupManager;
class GroupObject;
class ContactGroup;

class ContactGroupModelPrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(ContactGroupModel)

public:
    ContactGroupModel *q_ptr;

    ContactGroupModelPrivate(ContactGroupModel *parent);
    virtual ~ContactGroupModelPrivate();

    GroupManager *manager;
    QList<ContactGroup*> items;

    void setManager(GroupManager *manager);

    int indexForContacts(GroupObject *group);
    int indexForObject(GroupObject *group);

private slots:
    void groupAdded(GroupObject *group);
    void groupUpdated(GroupObject *group);
    void groupDeleted(GroupObject *group);

private:
    void itemDataChanged(int index);
    void addGroupToIndex(GroupObject *group, int index);
    void removeGroupFromIndex(GroupObject *group, int index);
};

}

ContactGroupModelPrivate::ContactGroupModelPrivate(ContactGroupModel *model)
        : QObject(model)
        , q_ptr(model)
        , manager(0)
{
}

ContactGroupModelPrivate::~ContactGroupModelPrivate()
{
    qDeleteAll(items);
    items.clear();
}

void ContactGroupModelPrivate::setManager(GroupManager *m)
{
    Q_Q(ContactGroupModel);

    if (manager == m)
        return;

    q->beginResetModel();

    if (manager) {
        disconnect(manager, 0, this, 0);
        disconnect(manager, 0, q, 0);

        foreach (ContactGroup *g, items)
            emit q->contactGroupRemoved(g);
        qDeleteAll(items);
        items.clear();
    }

    manager = m;

    if (manager) {
        connect(manager, SIGNAL(groupAdded(GroupObject*)), SLOT(groupAdded(GroupObject*)));
        connect(manager, SIGNAL(groupUpdated(GroupObject*)), SLOT(groupUpdated(GroupObject*)));
        connect(manager, SIGNAL(groupDeleted(GroupObject*)), SLOT(groupDeleted(GroupObject*)));
        connect(manager, SIGNAL(modelReady(bool)), q, SIGNAL(modelReady(bool)));
        connect(manager, SIGNAL(modelReady(bool)), q, SIGNAL(countChanged()));

        // Create data without sorting
        foreach (GroupObject *group, manager->groups()) {
            int index = indexForContacts(group);

            if (index < 0) {
                ContactGroup *item = new ContactGroup(this);
                index = items.size();
                items.append(item);
            }

            items[index]->addGroup(group);
            emit q->contactGroupCreated(items[index]);
        }

        std::sort(items.begin(), items.end(), contactGroupSort);
    }

    q->endResetModel();

    if (manager->isReady())
        emit q->modelReady(true);
}

int ContactGroupModelPrivate::indexForContacts(GroupObject *group)
{
    const RecipientList &searchRecipients = group->recipients();

    for (int i = 0; i < items.size(); i++) {
        int matched = 0;
        /* We have to match all groups to be sure that a contact change hasn't
         * invalidated the relationship */
        foreach (GroupObject *compareGroup, items[i]->groups()) {
            const RecipientList &compareRecipients = compareGroup->recipients();

            /* Multi-recipient groups are never combined, because that would create a
             * huge set of nasty corner cases, e.g. when two groups match in contacts
             * but not UIDs. */
            if (searchRecipients.size() > 1 || compareRecipients.size() > 1) {
                if (!searchRecipients.matches(compareRecipients))
                    break;
            } else if (!searchRecipients.hasSameContacts(compareRecipients)) {
                break;
            }

            matched++;
        }

        if (matched > 0 && matched == items[i]->groups().size())
            return i;
    }

    return -1;
}

int ContactGroupModelPrivate::indexForObject(GroupObject *group)
{
    for (int i = 0; i < items.size(); i++) {
        if (items[i]->groups().contains(group))
            return i;
    }

    return -1;
}

void ContactGroupModelPrivate::itemDataChanged(int index)
{
    Q_Q(ContactGroupModel);

    int newIndex = index;
    for (int i = index - 1; i >= 0; i--) {
        if (contactGroupSort(items[index], items[i]))
            newIndex = i;
        else
            break;
    }

    for (int i = index + 1; i < items.size(); i++) {
        if (contactGroupSort(items[i], items[index]))
            newIndex = i;
        else
            break;
    }

    if (newIndex != index) {
        q->beginMoveRows(QModelIndex(), index, index, QModelIndex(), newIndex > index ? newIndex + 1 : newIndex);
        items.move(index, newIndex);
        q->endMoveRows();
    }

    emit q->dataChanged(q->index(newIndex), q->index(newIndex));

    emit q->contactGroupChanged(items[newIndex]);
}

void ContactGroupModelPrivate::addGroupToIndex(GroupObject *group, int index)
{
    Q_Q(ContactGroupModel);

    ContactGroup *item = index < 0 ? new ContactGroup(this) : items[index];
    item->addGroup(group);

    if (index < 0) {
        for (index = 0; index < items.size(); index++) {
            if (contactGroupSort(item, items[index]))
                break;
        }

        q->beginInsertRows(QModelIndex(), index, index);
        items.insert(index, item);
        q->endInsertRows();

        emit q->contactGroupCreated(item);
        if (manager->isReady()) {
            emit q->countChanged();
        }
    } else {
        itemDataChanged(index);
    }
}

void ContactGroupModelPrivate::removeGroupFromIndex(GroupObject *group, int index)
{
    Q_Q(ContactGroupModel);

    ContactGroup *item = items[index];

    // Returns true when removing the last group
    if (item->removeGroup(group)) {
        emit q->beginRemoveRows(QModelIndex(), index, index);
        items.removeAt(index);
        emit q->endRemoveRows();

        emit q->contactGroupRemoved(item);
        if (manager->isReady()) {
            emit q->countChanged();
        }

        delete item;
    } else {
        itemDataChanged(index);
    }
}

void ContactGroupModelPrivate::groupAdded(GroupObject *group)
{
    int index = indexForContacts(group);
    addGroupToIndex(group, index);
}

void ContactGroupModelPrivate::groupUpdated(GroupObject *group)
{
    int oldIndex = indexForObject(group);
    int newIndex = -1;
    
    if (oldIndex >= 0) {
        newIndex = indexForContacts(group);

        if (oldIndex != newIndex) {
            // Remove from old
            removeGroupFromIndex(group, oldIndex);
        }
    }

    if (newIndex < 0 || oldIndex != newIndex) {
        // Add to new, creating if necessary
        addGroupToIndex(group, newIndex);
    } else {
        // Update data
        items[oldIndex]->updateGroup(group);
        itemDataChanged(oldIndex);
    }
}

void ContactGroupModelPrivate::groupDeleted(GroupObject *group)
{
    int index = indexForObject(group);
    if (index < 0)
        return;

    removeGroupFromIndex(group, index);
}

ContactGroupModel::ContactGroupModel(QObject *parent)
    : QAbstractListModel(parent),
      d(new ContactGroupModelPrivate(this))
{
}

QHash<int,QByteArray> ContactGroupModel::roleNames() const
{
    QHash<int,QByteArray> roles;
    roles[ContactIdsRole] = "contactIds";
    roles[ContactNamesRole] = "contactNames"; // TODO: Obsolete, remove
    roles[EndTimeRole] = "endTime";
    roles[UnreadMessagesRole] = "unreadMessages";
    roles[LastEventGroupRole] = "lastEventGroup";
    roles[LastEventIdRole] = "lastEventId";
    roles[LastMessageTextRole] = "lastMessageText";
    roles[LastVCardFileNameRole] = "lastVCardFileName";
    roles[LastVCardLabelRole] = "lastVCardLabel";
    roles[LastEventTypeRole] = "lastEventType";
    roles[LastEventStatusRole] = "lastEventStatus";
    roles[LastEventIsDraftRole] = "lastEventIsDraft";
    roles[LastModifiedRole] = "lastModified";
    roles[StartTimeRole] = "startTime";
    roles[GroupsRole] = "groups";
    roles[DisplayNamesRole] = "displayNames";
    roles[SubscriberIdentityRole] = "subscriberIdentity";
    roles[ContactGroupRole] = "contactGroup";
    roles[TimeSectionRole] = "timeSection";
    return roles;
}

ContactGroupModel::~ContactGroupModel()
{
    delete d;
    d = 0;
}

GroupManager *ContactGroupModel::manager() const
{
    return d->manager;
}

void ContactGroupModel::setManager(GroupManager *m)
{
    if (d->manager == m)
        return;

    d->setManager(m);
    emit managerChanged();
}

int ContactGroupModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return d->items.count();
}

QVariant ContactGroupModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= d->items.count()) {
        return QVariant();
    }

    ContactGroup *g = d->items.at(index.row());

    switch (role) {
    case ContactGroupRole:
        return QVariant::fromValue<QObject*>(g);
    case TimeSectionRole:
        return g->endTime().toLocalTime().date();
    case ContactIdsRole:
        return QVariant::fromValue(g->contactIds());
    case ContactNamesRole:
    case DisplayNamesRole:
        return QVariant::fromValue(g->displayNames());
    case GroupsRole:
        return QVariant::fromValue<QList<QObject*> >(g->groupObjects());
    case EndTimeRole:
        return g->endTime();
    case UnreadMessagesRole:
        return g->unreadMessages();
    case LastEventGroupRole:
        return QVariant::fromValue<QObject*>(g->lastEventGroup());
    case LastEventIdRole:
        return g->lastEventId();
    case LastMessageTextRole:
        return g->lastMessageText();
    case LastVCardFileNameRole:
        return g->lastVCardFileName();
    case LastVCardLabelRole:
        return g->lastVCardLabel();
    case LastEventTypeRole:
        return g->lastEventType();
    case LastEventStatusRole:
        return g->lastEventStatus();
    case LastEventIsDraftRole:
        return g->lastEventIsDraft();
    case LastModifiedRole:
        return g->lastModified();
    case StartTimeRole:
        return g->startTime();
    case SubscriberIdentityRole:
        return g->subscriberIdentity();
    default:
        qCWarning(lcCommHistory) << "ContactGroupModel::data: invalid role??" << role;
        return QVariant();
    }
}

int ContactGroupModel::count() const
{
    return d->items.count();
}

ContactGroup *ContactGroupModel::at(const QModelIndex &index) const
{
    return d->items.value(index.row());
}

QList<QObject*> ContactGroupModel::contactGroups() const
{
    QList<QObject*> re;
    re.reserve(d->items.size());

    foreach (ContactGroup *g, d->items)
        re.append(g);

    return re;
}

bool ContactGroupModel::canFetchMore(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return d->manager ? d->manager->canFetchMore() : false;
    return false;
}

void ContactGroupModel::fetchMore(const QModelIndex &parent)
{
    if (!parent.isValid() && d->manager)
        d->manager->fetchMore();
}

#include "contactgroupmodel.moc"

