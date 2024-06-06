/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2024 Damien Caliste <dcaliste@free.fr>
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

#include "updateslistener.h"
#include "dbus_p.h"

#include <QDBusConnection>
#include <QDBusMetaType>

namespace CommHistory {

UpdatesListener::UpdatesListener(QObject *parent)
    : UpdatesListener(COMM_HISTORY_OBJECT_PATH, parent)
{
}

UpdatesListener::UpdatesListener(const QString &objectPath, QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<CommHistory::Recipient>();
    qDBusRegisterMetaType<CommHistory::Event>();
    qDBusRegisterMetaType<CommHistory::Event::Contact>();
    qDBusRegisterMetaType<CommHistory::MessagePart>();
    qDBusRegisterMetaType<CommHistory::Group>();

    qDBusRegisterMetaType<QList<CommHistory::Event> >();
    qDBusRegisterMetaType<QList<CommHistory::Group> >();

    QDBusConnection::sessionBus().connect(
        QString(), objectPath, COMM_HISTORY_INTERFACE, EVENTS_ADDED_SIGNAL,
        this, SIGNAL(eventsAdded(const QList<CommHistory::Event> &)));
    QDBusConnection::sessionBus().connect(
        QString(), objectPath, COMM_HISTORY_INTERFACE, EVENTS_UPDATED_SIGNAL,
        this, SIGNAL(eventsUpdated(const QList<CommHistory::Event> &)));
    QDBusConnection::sessionBus().connect(
        QString(), objectPath, COMM_HISTORY_INTERFACE, EVENT_DELETED_SIGNAL,
        this, SIGNAL(eventDeleted(int)));

    QDBusConnection::sessionBus().connect(
        QString(), objectPath, COMM_HISTORY_INTERFACE, GROUPS_ADDED_SIGNAL,
        this, SIGNAL(groupsAdded(const QList<CommHistory::Group> &)));
    QDBusConnection::sessionBus().connect(
        QString(), objectPath, COMM_HISTORY_INTERFACE, GROUPS_UPDATED_SIGNAL,
        this, SIGNAL(groupsUpdated(const QList<int> &)));
    QDBusConnection::sessionBus().connect(
        QString(), objectPath, COMM_HISTORY_INTERFACE, GROUPS_UPDATED_FULL_SIGNAL,
        this, SIGNAL(groupsUpdatedFull(const QList<CommHistory::Group> &)));
    QDBusConnection::sessionBus().connect(
        QString(), objectPath, COMM_HISTORY_INTERFACE, GROUPS_DELETED_SIGNAL,
        this, SIGNAL(groupsDeleted(const QList<int> &)));
}

}
