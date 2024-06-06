/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2024 Jolla Ltd.
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

#ifndef DBUS_P_H
#define DBUS_P_H

#include <QDBusArgument>

#include "recipient.h"
#include "event.h"
#include "messagepart.h"
#include "group.h"

#define COMM_HISTORY_INTERFACE     QLatin1String("com.nokia.commhistory")
#define COMM_HISTORY_OBJECT_PATH   QLatin1String("/CommHistoryModel")

#define EVENTS_ADDED_SIGNAL        QLatin1String("eventsAdded")
#define EVENTS_UPDATED_SIGNAL      QLatin1String("eventsUpdated")
#define EVENT_DELETED_SIGNAL       QLatin1String("eventDeleted")

#define GROUPS_ADDED_SIGNAL        QLatin1String("groupsAdded")
#define GROUPS_UPDATED_SIGNAL      QLatin1String("groupsUpdated")
#define GROUPS_UPDATED_FULL_SIGNAL QLatin1String("groupsUpdatedFull")
#define GROUPS_DELETED_SIGNAL      QLatin1String("groupsDeleted")

QDBusArgument &operator<<(QDBusArgument &argument, const CommHistory::Recipient &recipient);
const QDBusArgument &operator>>(const QDBusArgument &argument, CommHistory::Recipient &recipient);

QDBusArgument &operator<<(QDBusArgument &argument, const CommHistory::RecipientList &recipients);
const QDBusArgument &operator>>(const QDBusArgument &argument, CommHistory::RecipientList &recipients);

QDBusArgument &operator<<(QDBusArgument &argument, const CommHistory::Event &event);
const QDBusArgument &operator>>(const QDBusArgument &argument, CommHistory::Event &event);

QDBusArgument &operator<<(QDBusArgument &argument, const CommHistory::Event::Contact &contact);
const QDBusArgument &operator>>(const QDBusArgument &argument, CommHistory::Event::Contact &contact);

QDBusArgument &operator<<(QDBusArgument &argument, const CommHistory::MessagePart &part);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                CommHistory::MessagePart &part);

QDBusArgument &operator<<(QDBusArgument &argument, const CommHistory::Group &group);
const QDBusArgument &operator>>(const QDBusArgument &argument, CommHistory::Group &group);

#endif
