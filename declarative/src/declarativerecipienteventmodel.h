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

#ifndef COMMHISTORY_DECLARATIVE_RECIPIENTEVENTMODEL_H
#define COMMHISTORY_DECLARATIVE_RECIPIENTEVENTMODEL_H

#include "recipienteventmodel.h"

#include <QQmlParserStatus>

class DeclarativeRecipientEventModelPrivate;

class DeclarativeRecipientEventModel : public CommHistory::RecipientEventModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(int contactId READ contactId WRITE setContactId NOTIFY contactIdChanged)
    Q_PROPERTY(QString remoteUid READ remoteUid WRITE setRemoteUid NOTIFY remoteUidChanged)

public:
    DeclarativeRecipientEventModel(QObject *parent = 0);

    int contactId() const;
    void setContactId(int contactId);

    QString remoteUid() const;
    void setRemoteUid(const QString &remoteUid);

    void classBegin() override;
    void componentComplete() override;

signals:
    void contactIdChanged();
    void remoteUidChanged();

private:
    void reload();

    QString m_remoteUid;
    int m_contactId = 0;
    bool m_complete = false;
};

#endif
