/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2014 Jolla Ltd.
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

#ifndef COMMHISTORY_RECIPIENT_H
#define COMMHISTORY_RECIPIENT_H

#include "libcommhistoryexport.h"

#include <QObject>
#include <QSharedPointer>
#include <QHash>
#include <QDebug>

namespace CommHistory {

/* TODO:
 * - Handling for multiple matching contacts on a recipient
 * - hidden/private numbers
 * - Outgoing events with multiple recipients
 * - Number normalization
 * - DB representation
 */

class RecipientPrivate;
typedef QWeakPointer<RecipientPrivate> WeakRecipient;

/* Represents one remote peer's address
 *
 * A recipient represents the pair of a localUid (local account address)
 * and remoteUid (remote peer's address on that account). It centralizes
 * logic for comparing addresses, display, and resolving matching contacts.
 *
 * Recipient's data is shared among instances which represent exactly the
 * same address, which ensures contact caching and consistent representation.
 *
 * Instances may be equal without being exactly identical, e.g. when using
 * minimized phone number comparisons. In that case, they may not be shared.
 */
class LIBCOMMHISTORY_EXPORT Recipient
{
public:
    struct PhoneNumberMatchDetails {
        QString number;
        QString minimizedNumber;
        quint32 minimizedNumberHash;
    };

    Recipient();
    Recipient(const QString &localUid, const QString &remoteUid);
    Recipient(const Recipient &o);
    Recipient(const WeakRecipient &weak);
    Recipient &operator=(const Recipient &o);
    ~Recipient();

    bool isNull() const;
 
    QString displayName() const;

    QString localUid() const;
    QString remoteUid() const;

    bool isPhoneNumber() const;
    QString minimizedPhoneNumber() const;
    QString minimizedRemoteUid() const;

    bool operator==(const Recipient &o) const;
    bool operator!=(const Recipient &o) const { return !operator==(o); }
    bool matches(const Recipient &o) const;
    bool isSameContact(const Recipient &o) const;

    bool matchesRemoteUid(const QString &remoteUid) const;
    bool matchesPhoneNumber(const PhoneNumberMatchDetails &phoneNumber) const;

    bool matchesAddressFlags(quint64 flags) const;

    int contactId() const;
    QString contactName() const;
    QUrl contactAvatarUrl() const;
    bool isContactResolved() const;

    /* Removes the resolved contact from this recipient
     *
     * Generally, this is only called by the contact listener.
     *
     * Unlike setResolved() called with a null item pointer, after this function
     * the contact is no longer considered resolved. Typically, resolution
     * will be attempted again, resulting in the recipient being resolved
     * once more.
     */
    void setUnresolved() const;

    /* Returns true if a contact update updates any property of the recipient
     *
     * Generally, this is only called by the contact listener.
     */
    bool contactUpdateIsSignificant() const;

    /* Get all existing recipients that are resolved to a contact ID
     *
     * This is primarily used for contact change notifications. contactId
     * may be 0, which returns all Recipient which have been resolved but
     * had no contact matches.
     */
    static QList<Recipient> recipientsForContact(int contactId);

    /* Return the string in the form suitable for testing phone number matches
     */
    static PhoneNumberMatchDetails phoneNumberMatchDetails(const QString &s);

    /* Create a Recipient from a given phone number
     */
    static Recipient fromPhoneNumber(const QString &number);

    /* Represent the current recipient as suitable for phone number matching
     */
    PhoneNumberMatchDetails toPhoneNumberMatchDetails() const;

private:
    QSharedPointer<RecipientPrivate> d;

    friend uint qHash(const CommHistory::Recipient &value, uint seed);
    friend RecipientPrivate;
};

class LIBCOMMHISTORY_EXPORT RecipientList
{
public:
    RecipientList();
    RecipientList(const Recipient &recipient);
    RecipientList(const QList<Recipient> &recipients);

    static RecipientList fromUids(const QString &localUid, const QStringList &remoteUids);
    static RecipientList fromPhoneNumbers(const QStringList &phoneNumbers);
    static RecipientList fromContact(int contactId);

    bool isEmpty() const;
    int size() const;
    int count() const { return size(); }

    QList<Recipient> recipients() const;
    QStringList displayNames() const;
    QList<int> contactIds() const;

    QStringList remoteUids() const;
    
    bool operator==(const RecipientList &o) const;
    bool operator!=(const RecipientList &o) const { return !operator==(o); }
    bool matches(const RecipientList &o) const;
    bool hasSameContacts(const RecipientList &o) const;

    bool matchesRemoteUid(const QString &remoteUid) const
    {
        for (const_iterator it = constBegin(), end = constEnd(); it != end; ++it)
            if (it->matchesRemoteUid(remoteUid))
                return true;

        return false;
    }

    bool matchesPhoneNumber(const Recipient::PhoneNumberMatchDetails &phoneNumber) const
    {
        for (const_iterator it = constBegin(), end = constEnd(); it != end; ++it)
            if (it->matchesPhoneNumber(phoneNumber))
                return true;

        return false;
    }

    template<typename Sequence>
    bool intersects(const Sequence &sequence) const
    {
        for (typename Sequence::const_iterator it = sequence.constBegin(), end = sequence.constEnd(); it != end; ++it)
            if (contains(*it))
                return true;

        return false;
    }

    template<typename Sequence>
    bool intersectsMatch(const Sequence &sequence) const
    {
        for (typename Sequence::const_iterator it = sequence.constBegin(), end = sequence.constEnd(); it != end; ++it)
            if (containsMatch(*it))
                return true;

        return false;
    }

    bool allContactsResolved() const;

    QString debugString() const;

    typedef QList<Recipient>::const_iterator const_iterator;
    typedef QList<Recipient>::iterator iterator;

    iterator begin();
    iterator end();
    iterator find(const Recipient &r);
    iterator findMatch(const Recipient &r);

    const_iterator begin() const { return constBegin(); }
    const_iterator end() const { return constEnd(); }

    const_iterator constBegin() const;
    const_iterator constEnd() const;
    const_iterator constFind(const Recipient &r) const;
    const_iterator constFindMatch(const Recipient &r) const;

    bool contains(const Recipient &r) const;
    bool containsMatch(const Recipient &r) const;

    Recipient value(int index) const;
    const Recipient &at(int index) const;
    const Recipient &first() const { return at(0); }

    void append(const Recipient &r);
    void append(const QList<Recipient> &o);

    RecipientList &unite(const RecipientList &other);

    RecipientList &operator<<(const Recipient &recipient);

private:
    QList<Recipient> m_recipients;
};

inline uint qHash(const CommHistory::Recipient &value, uint seed = 0)
{
    return ::qHash(value.d.data(), seed);
}

}

LIBCOMMHISTORY_EXPORT QDebug &operator<<(QDebug &debug, const CommHistory::Recipient &recipient);
LIBCOMMHISTORY_EXPORT QDebug &operator<<(QDebug &debug, const CommHistory::RecipientList &recipientList);

Q_DECLARE_METATYPE(CommHistory::Recipient)
Q_DECLARE_METATYPE(CommHistory::RecipientList)

#endif

