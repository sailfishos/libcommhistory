#ifndef RECIPIENT_P_H
#define RECIPIENT_P_H

#include <seasidecache.h>

#include <QContactId>

#include <QString>
#include <QSharedPointer>

#include "recipient.h"

namespace CommHistory {

class RecipientPrivate
{
public:
    QString localUid;
    QString remoteUid;
    SeasideCache::CacheItem* item;
    bool isResolved;
    bool isPhoneNumber;
    QString minimizedRemoteUid;
    quint32 localUidHash;
    quint32 remoteUidHash;
    quint32 contactNameHash;
    quint32 addressFlags;

    RecipientPrivate(const QString &localUid, const QString &remoteUid);
    ~RecipientPrivate();

    /* Update the resolved contact for this recipient
     *
     * Generally, this is only called by the contact resolver, but it can be
     * used to inject known contact matches. The change will apply to all
     * Recipient instances that compare equal to this instance.
     *
     * A null item pointer is taken to mean that no contact matches. In this
     * case, the contact is still considered resolved.
     *
     * Returns true if the receipient resolution was updated.
     */
    static bool setResolved(const Recipient *q, SeasideCache::CacheItem *item);

    static QSharedPointer<RecipientPrivate> get(const QString &localUid, const QString &remoteUid);
    static RecipientList recipientListFromCacheItem(const SeasideCache::CacheItem *item);
    static RecipientList recipientListFromContact(const QContactId &contactId);
};

}

#endif
