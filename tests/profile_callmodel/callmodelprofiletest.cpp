/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2015 Jolla Ltd.
** Contact: Matt Vogt <matthew.vogt@jollamobile.com>
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

#include "callmodelprofiletest.h"
#include "callmodel.h"
#include "common.h"
#include <QtTest/QtTest>
#include <QDateTime>
#include <QDBusConnection>
#include <QElapsedTimer>
#include <QThread>
#include <cstdlib>

using namespace CommHistory;

void CallModelProfileTest::initTestCase()
{
    initTestDatabase();

    logFile = 0;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    srand( QDateTime::currentDateTime().toSecsSinceEpoch() );
#else
    qsrand( QDateTime::currentDateTime().toTime_t() );
#endif
}

void CallModelProfileTest::init()
{
}

void CallModelProfileTest::prepare()
{
    const int events = 3000;
    const int contacts = 500;
    const int selected = 500;

    deleteAll();

    int commitBatchSize = 100;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QRandomGenerator qrand;
#endif
    EventModel addModel;
    QDateTime when = QDateTime::currentDateTime();

    qDebug() << Q_FUNC_INFO << "- Creating" << contacts << "new contacts";

    QList<QPair<QString, QPair<QString, QString> > > contactDetails;

    int ci = remoteUids.count();
    while(ci < contacts) {
        QString phoneNumber;
        do {
            phoneNumber = QString().setNum(qrand() % 10000000);
        } while (remoteUids.contains(phoneNumber));
        remoteUids << phoneNumber;
        contactIndices << ci;
        ci++;

        contactDetails.append(qMakePair(QString("Test Contact %1").arg(ci), qMakePair(phoneNumber, QString())));

        if(ci % commitBatchSize == 0 && ci < contacts) {
            qDebug() << Q_FUNC_INFO << "- adding" << commitBatchSize
                << "contacts (" << ci << "/" << contacts << ")";
            addTestContacts(contactDetails);
            contactDetails.clear();
        }
    }
    if (!contactDetails.isEmpty()) {
        qDebug() << Q_FUNC_INFO << "- adding rest of the contacts ("
                 << ci << "/" << contacts << ")";
        addTestContacts(contactDetails);
        contactDetails.clear();
    }

    // Randomize the contact indices
    random_shuffle(contactIndices.begin(), contactIndices.end());

    qDebug() << Q_FUNC_INFO << "- Creating" << events << "new events";

    QList<Event> eventList;

    int ei = 0;
    while(ei < events) {
        ei++;

        Event::EventDirection direction;
        bool isMissed = false;

        if(qrand() % 2 > 0) {
            direction = Event::Inbound;
            isMissed = (qrand() % 2 > 0);
        } else {
            direction = Event::Outbound;
        }

        Event e;
        e.setType(Event::CallEvent);
        e.setDirection(direction);
        e.setGroupId(-1);
        e.setStartTime(when.addSecs(ei));
        e.setEndTime(when.addSecs(ei));
        e.setLocalUid(RING_ACCOUNT);
        e.setRecipients(Recipient::fromPhoneNumber(remoteUids.at(contactIndices.at(qrand() % selected))));
        e.setFreeText("");
        e.setIsDraft(false);
        e.setIsMissedCall(isMissed);

        eventList << e;

        if(ei % commitBatchSize == 0 && ei != events) {
            qDebug() << Q_FUNC_INFO << "- adding" << commitBatchSize
                << "events (" << ei << "/" << events << ")";
            QVERIFY(addModel.addEvents(eventList, false));
            eventList.clear();
        }
    }

    QVERIFY(addModel.addEvents(eventList, false));
    qDebug() << Q_FUNC_INFO << "- adding rest of the events ("
        << ei << "/" << events << ")";
    eventList.clear();
}

void CallModelProfileTest::execute()
{
    const bool resolve = false;

    QList<int> times;

    int iterations = 1;

    logFile = new QFile("libcommhistory-performance-test.log");
    if(!logFile->open(QIODevice::Append)) {
        qDebug() << "!!!! Failed to open log file !!!!";
        logFile = 0;
    }

    QDateTime startTime = QDateTime::currentDateTime();

    qDebug() << Q_FUNC_INFO << "- Fetching events." << iterations << "iterations";
    for(int i = 0; i < iterations; i++) {

        CallModel fetchModel;

        fetchModel.setResolveContacts(resolve ? EventModel::ResolveImmediately : EventModel::DoNotResolve);
        fetchModel.setFilter(CallModel::SortByContact);

        QElapsedTimer time;
        time.start();

        bool result = fetchModel.getEvents();
        QVERIFY(result);

        if (!fetchModel.isReady())
            waitForSignal(&fetchModel, SIGNAL(modelReady(bool)));

        int elapsed = time.elapsed();
        times << elapsed;
        qDebug("Time elapsed: %d ms", elapsed);

        QVERIFY(fetchModel.rowCount() > 0);
    }

    summarizeResults(metaObject()->className(), times, logFile, startTime.secsTo(QDateTime::currentDateTime()));
}

void CallModelProfileTest::finalise()
{
    deleteAll();
}

void CallModelProfileTest::cleanupTestCase()
{
    if (logFile) {
        logFile->close();
        delete logFile;
        logFile = 0;
    }
}

QTEST_MAIN(CallModelProfileTest)
