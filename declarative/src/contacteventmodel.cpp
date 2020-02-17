/* Copyright (C) 2015 Jolla Ltd.
 * Contact: Matt Vogt <matthew.vogt@jollamobile.com>
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

#include "contacteventmodel.h"
#include "commonutils.h"
#include <QTimer>

using namespace CommHistory;

ContactEventModel::ContactEventModel(QObject *parent)
    : SingleContactEventModel(parent)
    , m_contactId(0)
{
}

void ContactEventModel::setContactId(int contactId)
{
    if (contactId == m_contactId)
        return;

    m_contactId = contactId;
    emit contactIdChanged();

    QTimer::singleShot(0, this, SLOT(reload()));
}

void ContactEventModel::setFallbackPhoneId(const QString &fallbackPhoneId)
{
    if (fallbackPhoneId == m_fallbackPhoneId)
        return;

    m_fallbackPhoneId = fallbackPhoneId;
    emit fallbackPhoneIdChanged();

    if (!m_contactId) {
        QTimer::singleShot(0, this, SLOT(reload()));
    }
}

void ContactEventModel::reload()
{
    if (m_contactId > 0) {
        getEvents(m_contactId);
    } else if (!m_fallbackPhoneId.isEmpty()) {
        // LocalUID is set to QString because we don't care about which sim
        // the call was received on.
        // SingleContactEventModel::getEvents(Recipient) also exists and
        // is matching a stored contact from recipient phone number. We actually
        // need here to get events for a recipient event if not saved, so we
        // must call the RecipientEventModel::getEvents() explicitely to avoid
        // calling implicitely the SingleContactEventModel variant.
        RecipientEventModel::getEvents(Recipient(QString(), m_fallbackPhoneId));
    }
}
