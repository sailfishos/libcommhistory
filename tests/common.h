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

#ifndef COMMON_H
#define COMMON_H

#include "commonutils.h"
#include "commonutils_p.h"
#include "eventmodel.h"
#include "event.h"
#include "group.h"

#include <seasidecache.h>

#include <QTimer>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>

#include <iterator>

namespace CommHistory {

template <class RandomAccessIterator>
void random_shuffle (RandomAccessIterator first, RandomAccessIterator last)
{
    typename std::iterator_traits<RandomAccessIterator>::difference_type i, n;
    n = (last-first);
    for (i=n-1; i>0; --i) {
        qSwap(first[i],first[qrand() % (i+1)]);
    }
}

void waitForSignal(QObject *object, const char *signal);

class ContactChangeListener : public SeasideCache::ChangeListener
{
public:
    ContactChangeListener();
    ~ContactChangeListener();

    bool waitForContactAdded(int contactId);
    bool waitForContactDeleted(int contactId);

private:
    bool waitForChange(quint32 contactId, QSet<quint32> *changeWaits, QSet<quint32> *changeRecords);
    void itemUpdated(SeasideCache::CacheItem *item);
    void itemAboutToBeRemoved(SeasideCache::CacheItem *item);

    QSet<quint32> m_updatedContactIds;
    QSet<quint32> m_deletedContactIds;
    QSet<quint32> m_waitForContactAddedId;
    QSet<quint32> m_waitForContactDeletedId;
    QEventLoop *m_loop = nullptr;
    QTimer *m_timer = nullptr;
};

}

using namespace CommHistory;

const QString TEST_DATABASE_DIR = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + "/tests";

const QString ACCOUNT1 = "/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0";
const QString ACCOUNT2 = "/org/freedesktop/Telepathy/Account/gabble/jabber/dut2_40localhost0";

const int WAIT_SIGNAL_TIMEOUT = 5000;

/* Duration of phone calls added with addTestEvent */
const int TESTCALL_SECS = 100;

void initTestDatabase();

int addTestEvent(EventModel &model,
                 Event::EventType type,
                 Event::EventDirection direction,
                 const QString &account,
                 int groupId,
                 const QString &text = QString("test event"),
                 bool isDraft = false,
                 bool isMissedCall = false,
                 const QDateTime &when = QDateTime::currentDateTime(),
                 const QString &remoteUid = QString(),
                 bool toModelOnly = false,
                 const QString &messageToken = QString(),
                 const QString &subscriberIdentity = QString());

void addTestGroups(Group &group1, Group &group2);
void addTestGroup(Group& grp, QString localUid, QString remoteUid);
int addTestContact(const QString &name, const QString &remoteUid, const QString &localUid=QString(), ContactChangeListener *listener = nullptr);
QList<int> addTestContacts(const QList<QPair<QString, QPair<QString, QString> > > &details);
bool addTestContactAddress(int contactId, const QString &remoteUid, const QString &localUid=QString());
void modifyTestContact(int id, const QString &name, bool favorite = false);
void deleteTestContact(int id, ContactChangeListener *listener = nullptr);
QContactId localContactForAggregate(const QContactId &contactId);
void cleanupTestGroups();
void cleanupTestEvents();
bool compareEvents(Event &e1, Event &e2);
void deleteAll(bool deleteDb = true);
QString randomMessage(int words);
void summarizeResults(const QString &className, QList<int> &times, QFile *logFile, int testSecs);

#endif
