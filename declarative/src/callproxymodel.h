/* Copyright (C) 2012 Tom Swindell <t.swindell@rubyx.co.uk>
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
#ifndef CALLPROXYMODEL_H
#define CALLPROXYMODEL_H

#include <QQmlParserStatus>

#include "callmodel.h"

class CallProxyModel : public CommHistory::CallModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    Q_ENUMS(EventRole)
    Q_ENUMS(EventType)
    Q_ENUMS(EventDirection)
    Q_ENUMS(EventStatus)
    Q_ENUMS(EventReadStatus)
    Q_ENUMS(GroupBy)

    Q_PROPERTY(GroupBy groupBy READ groupBy WRITE setGroupBy NOTIFY groupByChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool resolveContacts READ resolveContacts WRITE setResolveContacts NOTIFY resolveContactsChanged)
    Q_PROPERTY(bool populated READ populated NOTIFY populatedChanged)
    Q_PROPERTY(int _limit READ limit WRITE setLimit NOTIFY limitChanged)

public:
    enum EventRole {
        EventIdRole = CommHistory::EventModel::BaseRole,
        EventTypeRole,
        StartTimeRole,
        EndTimeRole,
        DirectionRole,
        IsDraftRole,
        IsReadRole,
        IsMissedCallRole,
        StatusRole,
        BytesReceivedRole,
        LocalUidRole,
        RemoteUidRole,
        ContactsRole,
        FreeTextRole,
        GroupIdRole,
        MessageTokenRole,
        LastModifiedRole,
        EventCountRole,
        FromVCardFileNameRole,
        FromVCardLabelRole
    };

    enum EventType {
        UnknownType = 0,
        IMEvent,
        SMSEvent,
        CallEvent,
        VoicemailEvent,
        StatusMessageEvent,
        MMSEvent,
        ClassZeroSMSEvent
    };

    enum EventDirection {
        UnknownDirection = 0,
        Inbound,
        Outbound
    };

    enum EventStatus {
        UnknownStatus = 0,
        SendingStatus,
        SentStatus,
        DeliveredStatus,
        FailedStatus,
        TemporarilyFailedStatus = FailedStatus,
        PermanentlyFailedStatus,
        TemporarilyFailedOfflineStatus
    };

    enum EventReadStatus {
        UnknownReadStatus = 0,
        ReadStatusRead,
        ReadStatusDeleted
    };

    enum GroupBy
    {
        GroupByNone = CommHistory::CallModel::SortByTime,
        GroupByContact = CommHistory::CallModel::SortByContact,
        GroupByContactAndType = CommHistory::CallModel::SortByContactAndType
    };

    explicit CallProxyModel(QObject *parent = 0);

    void classBegin();
    void componentComplete();

    GroupBy groupBy() const;
    void setGroupBy(GroupBy grouping);

    int count() const;

    // Shadow CallModel functions:
    int limit() const;
    void setLimit(int);

    // Shadow CallModel functions:
    bool resolveContacts() const;
    void setResolveContacts(bool enabled);

    bool populated() const;

    Q_INVOKABLE int createOutgoingCallEvent(const QString &localUid, const QString &remoteUid);

public Q_SLOTS:
    void deleteAt(int index);

    // Shadow CallModel function:
    bool markAllRead();

private slots:
    void onReadyChanged(bool ready);

Q_SIGNALS:
    void groupByChanged();
    void countChanged();
    void resolveContactsChanged();
    void populatedChanged();
    void limitChanged();

private:
    GroupBy m_grouping;
    int m_limit;
    bool m_resolveContacts;
    bool m_componentComplete;
    bool m_populated;
};

#endif // CALLPROXYMODEL_H
