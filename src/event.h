/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2014-2016 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: John Brooks <john.brooks@jolla.com>
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

#ifndef COMMHISTORY_EVENT_H
#define COMMHISTORY_EVENT_H

#include <QSharedDataPointer>
#include <QString>
#include <QUrl>
#include <QDateTime>
#include <QVariant>
#include <QSet>

#include "messagepart.h"
#include "recipient.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class EventPrivate;

/*!
 * \class Event
 *
 * Instant message, SMS or call data.
 */
class LIBCOMMHISTORY_EXPORT Event
{
    Q_GADGET
    Q_PROPERTY(int id READ id CONSTANT)
    Q_PROPERTY(QDateTime startTime READ startTime CONSTANT)
    Q_PROPERTY(CommHistory::Event::EventDirection direction READ direction CONSTANT)
    Q_PROPERTY(QString localUid READ localUid CONSTANT)
    Q_PROPERTY(QString dateAndAccountGrouping READ dateAndAccountGrouping CONSTANT)

public:
    enum EventType {
        UnknownType = 0,
        IMEvent,
        SMSEvent,
        CallEvent,
        VoicemailEvent,
        /*!
         * Type of the events which represent the status message of the remote party in the
         * conversation. These events must be added to model with toModelOnly=true option. Note that
         * all items which are added to data model but not saved to database, are lost on the next
         * fetching.
         */
        StatusMessageEvent,
        MMSEvent,
        ClassZeroSMSEvent
    };

    enum EventCategory {
        AnyCategory                 = 0,
        VoicecallCategory           = (1 << 0),
        VoicemailCategory           = (1 << 1),
        ShortMessagingCategory      = (1 << 2),
        MultimediaMessagingCategory = (1 << 3),
        InstantMessagingCategory    = (1 << 4),
        OtherCategory               = (1 << 5)
    };

    enum EventDirection {
        UnknownDirection = 0,
        Inbound,
        Outbound
    };
    Q_ENUM(EventDirection)

    enum EventStatus {
        ManualNotificationStatus = -4,
        WaitingStatus = -3,
        DownloadingStatus = -2,
        ReceivedStatus = -1,
        UnknownStatus = 0,
        SendingStatus,
        SentStatus,
        DeliveredStatus,
        // deprecate FailedStatus
        FailedStatus,
        TemporarilyFailedStatus = FailedStatus,
        PermanentlyFailedStatus,
        TemporarilyFailedOfflineStatus
    };

    enum EventReadStatus {
        UnknownReadStatus = 0,
        ReadStatusRead    = 1,
        ReadStatusDeleted = 2
    };

    enum EventIncomingStatus {
        Received = 0,
        NotAnswered,  // Set the IsMissedCall property
        Ignored,      // Set the IsMissedCall property
        Rejected      // Set the IsMissedCall property
    };
    Q_ENUM(EventIncomingStatus)

    enum Property {
        Id = 0,        // always valid
        Type,          // always valid
        StartTime,
        EndTime,
        Direction,
        IsDraft,
        IsRead,
        IsMissedCall,
        IsEmergencyCall,
        Status,
        BytesReceived,
        LocalUid,
        RemoteUid, // TODO: remove, deprecated
        ContactId, // TODO: remove
        ContactName, // TODO: remove
        Subject,
        FreeText,
        GroupId,
        MessageToken,
        LastModified,
        EventCount,
        FromVCardFileName,
        FromVCardLabel,
        ReportDelivery,
        ValidityPeriod,
        ContentLocation,
        MessageParts,
        ReadStatus,
        ReportRead,
        ReportReadRequested,
        MmsId,
        Contacts,
        IsAction,
        Headers,
        ExtraProperties,
        Recipients,
        IsResolved,
        //
        NumProperties
    };

    typedef QSet<Event::Property> PropertySet;

    // FIXME: potential risk of QContactLocalId (quint32) not fitting to int.
    // should we change event/group.contactId to uint?
    typedef QPair<int, QString> Contact;

public:
    Event();
    Event(const Event &other);
    ~Event();

    Event &operator=(const Event &other);
    bool operator==(const Event &other) const;
    bool operator!=(const Event &other) const;
    bool isValid() const;

    /*!
     * Extracts event id from tracker url.
     */
    static int urlToId(const QString &url);

    /*!
     * Translate event id to tracker url.
     */
    static QUrl idToUrl(int id);

    /*!
     * Returns a property set with all properties.
     */
    static Event::PropertySet allProperties();

    /*!
     * Get set of valid properties (i.e. properties that have been
     * assigned a value since the event was created) for this event.
     *
     * \return Set of valid properties.
     */
    Event::PropertySet validProperties() const;

    /*!
     * Set valid properties. API users should not normally need this, as
     * properties are automatically marked valid when modified.
     *
     * \param properties New set of properties.
     */
    void setValidProperties(const Event::PropertySet &properties);

    /*!
     * Get set of modified properties. It's usually reset after addEvent
     * or modifyEvent.
     *
     * \return Set of modified properties.
     */
    Event::PropertySet modifiedProperties() const;

    /*!
     * Reset modified properties. API users should not normally need this.
     *
     * \param properties New set of properties.
     */
    void resetModifiedProperties();

    QVariantMap extraProperties() const;
    void setExtraProperties(const QVariantMap &extraProperties);
    QVariant extraProperty(const QString &key) const;
    void setExtraProperty(const QString &key, const QVariant &value);
    void removeExtraProperty(const QString &key);

    //\\//\\// G E T - A C C E S S O R S //\\//\\//
    int id() const;
    QUrl url() const;
    Event::EventType type() const;
    Event::EventCategory category() const;

    /*!
     * Gets the start time of a call, sent/sending/failed time of SMS/IM
     *
     * \return Timestamp.
     */
    QDateTime startTime() const;

    /*!
     * Gets the end time of a call, sent/sending/failed time of  SMS/IM (if delivery reports are enabled: delivered time of SMS)
     *
     * \return Timestamp.
     */
    QDateTime endTime() const;

    Event::EventDirection direction() const;
    bool isDraft() const;
    bool isRead() const;
    bool isMissedCall() const;
    bool isEmergencyCall() const;
    bool isVideoCall() const;
    Event::EventStatus status() const;
    int bytesReceived() const;
    QString localUid() const;
    QString dateAndAccountGrouping() const;
    EventIncomingStatus incomingStatus() const;

    const RecipientList &recipients() const;
    RecipientList contactRecipients() const;

    /* DEPRECATED - use contacts(). Returns the id of the first matching contact. */
    int contactId() const;

    /* DEPRECATED - use contacts(). Returns the name of the first matching contact. */
    QString contactName() const;

    QList<Event::Contact> contacts() const;
    QString subject() const;
    QString freeText() const;
    int groupId() const; /* communication channel */
    QString messageToken() const;
    QString mmsId() const;
    QDateTime lastModified() const;

    /*!
     * \brief Gets how many similar call events are represented by the current event.
     *
     * This property gets how many call events are grouped under the current one.
     * The value is set and used by the CallHistoryCallModel and call-history
     * application.
     *
     * NOTE: This property has meaningful value only in case of grouped call
     * events. In all other case, the value should be ignored.
     *
     * \return The number of similar events in the same call event group.
     */
    int eventCount() const;

    QString fromVCardFileName() const;
    QString fromVCardLabel() const;
    bool reportDelivery() const;
    bool reportRead() const;
    bool reportReadRequested() const;
    Event::EventReadStatus readStatus() const;
    int validityPeriod() const;

    // MMS content location URL, for MMS notifications
    QString contentLocation() const;

    QList<MessagePart> messageParts() const;

    // NOTE: Cc and Bcc will not be initialized in a getEvents() model
    // query. You have to fetch the full event data with getEvent() to
    // access these properties.
    // toList() is a convenience method for getting the "x-mms-to"
    // header contents; also available from headers() with addresses
    // separated by \x1e.
    QStringList toList() const;
    QStringList ccList() const;
    QStringList bccList() const;

    // Action chat messages (e.g. "/me is happy"), supported only for IMEvent
    bool isAction() const;

    // True if the recipients in this event have all been resolved
    bool isResolved() const;

    // Optional message headers, key/value.
    QHash<QString, QString> headers() const;

    quint32 startTimeT() const;
    quint32 endTimeT() const;
    quint32 lastModifiedT() const;

    QString subscriberIdentity() const;

    //\\//\\// S E T - A C C E S S O R S //\\//\\//
    void setId(int id);
    void setType(Event::EventType type);
    void setStartTime(const QDateTime &startTime);
    void setEndTime(const QDateTime &endTime);
    void setDirection(Event::EventDirection direction);
    void setIsDraft(bool isDraft);
    void setIsRead(bool isRead);
    void setIsMissedCall(bool isMissed);
    void setIsEmergencyCall(bool isEmergency);
    void setIsVideoCall(bool isVideo);
    void setStatus(Event::EventStatus status);
    void setBytesReceived(int bytes);
    void setLocalUid(const QString &uid);
    void setRecipients(const RecipientList &recipients);
    void setSubject(const QString &subject);
    void setFreeText(const QString &text);
    void setGroupId(int id);
    void setMessageToken(const QString &token);
    void setMmsId(const QString &id);
    void setLastModified(const QDateTime &modified);
    void setIncomingStatus(EventIncomingStatus status);

    /*!
     * \brief Sets the value of how many similar call events are represented by the current event.
     *
     * NOTE: This property has meaningful value only in case of grouped call
     * events. In all other case, the value should be ignored.
     *
     * \return The number of similar events in the same call event group.
     */
    void setEventCount(int count);
    void setFromVCard(const QString &fileName, const QString &label = QString()); //fromvcard
    void setReportDelivery(bool reportDeliveryRequested);
    void setReportRead(bool reportRequested);
    void setReportReadRequested(bool reportShouldRequested);
    void setReadStatus(Event::EventReadStatus eventReadStatus);
    void setValidityPeriod(int validity);
    void setContentLocation(const QString &location);
    void setMessageParts(const QList<MessagePart> &parts);
    void addMessagePart(const MessagePart &part);

    // Convenience method for setting the "x-mms-to" header.
    void setToList(const QStringList &toList);
    void setCcList(const QStringList &ccList);
    void setBccList(const QStringList &bccList);

    void setIsAction(bool isAction);
    void setIsResolved(bool isResolved);
    void setHeaders(const QHash<QString, QString> &headers);
    void setStartTimeT(quint32 t);
    void setEndTimeT(quint32 t);
    void setLastModifiedT(quint32 t);
    void setSubscriberIdentity(const QString &id);

    QString toString() const;

    bool resetModifiedProperty(Event::Property property);

    /*!
     * \brief Copy all valid properties from other event
     *
     * \param another event
     */
    void copyValidProperties(const Event &other);

    static quint32 currentTime_t() { return QDateTime::currentDateTimeUtc().toTime_t(); }

private:
    QSharedDataPointer<EventPrivate> d;
};

}

LIBCOMMHISTORY_EXPORT QDataStream &operator<<(QDataStream &stream, const CommHistory::Event &event);
LIBCOMMHISTORY_EXPORT QDataStream &operator>>(QDataStream &stream, CommHistory::Event &event);

Q_DECLARE_METATYPE(CommHistory::Event)
Q_DECLARE_METATYPE(QList<CommHistory::Event>)
Q_DECLARE_METATYPE(CommHistory::Event::Contact)
Q_DECLARE_METATYPE(QList<CommHistory::Event::Contact>)
Q_DECLARE_METATYPE(CommHistory::Event::EventDirection)

#endif
