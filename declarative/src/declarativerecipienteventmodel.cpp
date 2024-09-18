/*
 * Copyright (C) 2015 - 2019 Jolla Ltd.
 * Copyright (C) 2020 Open Mobile Platform LLC.
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "declarativerecipienteventmodel.h"
#include "commonutils_p.h"

#include <QQmlInfo>

using namespace CommHistory;


DeclarativeRecipientEventModel::DeclarativeRecipientEventModel(QObject *parent)
    : RecipientEventModel(parent)
{
}

void DeclarativeRecipientEventModel::setContactId(int contactId)
{
    if (contactId == m_contactId)
        return;

    if (!m_remoteUid.isEmpty()) {
        qmlInfo(this) << "remoteUid already set to" << m_remoteUid
                      << ", ignoring contactId change to" << contactId;
        return;
    }

    m_contactId = contactId;
    reload();
    emit contactIdChanged();
}

int DeclarativeRecipientEventModel::contactId() const
{
    return m_contactId;
}

void DeclarativeRecipientEventModel::setRemoteUid(const QString &remoteUid)
{
    if (remoteUid == m_remoteUid)
        return;

    if (m_contactId > 0) {
        qmlInfo(this) << "contactId already set to" << m_contactId
                      << ", ignoring remoteUid change to" << remoteUid;
        return;
    }

    m_remoteUid = remoteUid;
    reload();
    emit remoteUidChanged();
}

QString DeclarativeRecipientEventModel::remoteUid() const
{
    return m_remoteUid;
}

void DeclarativeRecipientEventModel::classBegin()
{
}

void DeclarativeRecipientEventModel::componentComplete()
{
    m_complete = true;
    reload();
}

void DeclarativeRecipientEventModel::reload()
{
    if (!m_complete)
        return;

    if (m_contactId > 0) {
        setRecipients(m_contactId);
    } else if (!m_remoteUid.isEmpty()) {
        // Use the generic RING_ACCOUNT because we don't care about which SIM
        // the call was received on.
        setRecipients(Recipient(RING_ACCOUNT, m_remoteUid));
    }

    getEvents();
}
