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
#include <QCoreApplication>

#include "adaptor.h"

#include "updatesemitter.h"
#include "dbus_p.h"
#include "debug_p.h"

namespace CommHistory {

QWeakPointer<UpdatesEmitter> UpdatesEmitter::m_Instance;
QString m_serviceName;

UpdatesEmitter::UpdatesEmitter()
{
    new Adaptor(this);
    if (!QDBusConnection::sessionBus().registerObject(COMM_HISTORY_OBJECT_PATH,
                                                      this)) {
        qCWarning(lcCommHistory) << Q_FUNC_INFO << ": error registering object";
    }
    m_serviceName
        = QString::fromLatin1("%1.p%2").arg(COMM_HISTORY_SERVICE_NAME_PREFIX,
                                            QCoreApplication::applicationPid());
    if (!QDBusConnection::sessionBus().registerService(m_serviceName)) {
        qCWarning(lcCommHistory) << Q_FUNC_INFO << ": error registering service"
                                 << QDBusConnection::sessionBus().lastError();
    }
}

UpdatesEmitter::~UpdatesEmitter()
{
    QDBusConnection::sessionBus().unregisterObject(COMM_HISTORY_OBJECT_PATH);
    QDBusConnection::sessionBus().unregisterService(m_serviceName);
}

QSharedPointer<UpdatesEmitter> UpdatesEmitter::instance()
{
    QSharedPointer<UpdatesEmitter> result;
    if (!m_Instance) {
        result = QSharedPointer<UpdatesEmitter>(new UpdatesEmitter());
        m_Instance = result.toWeakRef();
    } else {
        result = m_Instance.toStrongRef();
    }

    return result;
}

}
