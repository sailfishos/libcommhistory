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

#include <QtTest/QtTest>
#include <QDBusConnection>
#include "conversationmodeltest.h"
#include "groupmodel.h"
#include "conversationmodel.h"
#include "adaptor.h"
#include "event.h"
#include "common.h"
#include "databaseio.h"
#include "modelwatcher.h"

using namespace CommHistory;

Group group1, group2;
QEventLoop *loop;

ModelWatcher watcher, watcher2;

void ConversationModelTest::initTestCase()
{
    initTestDatabase();
    QVERIFY(QDBusConnection::sessionBus().isConnected());

    loop = new QEventLoop(this);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    srand(QDateTime::currentDateTime().toSecsSinceEpoch());
#else
    qsrand(QDateTime::currentDateTime().toTime_t());
#endif

    addTestGroups(group1, group2);

    EventModel model;
    watcher.setModel(&model);
    addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT1, group1.id());
    addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT1, group1.id());
    addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1, group1.id());
    addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1, group1.id());

    addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT2, group1.id());
    addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT2, group1.id());

    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1, group1.id());
    addTestEvent(model, Event::SMSEvent, Event::Outbound, ACCOUNT1, group1.id());

    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT2, group1.id());
    addTestEvent(model, Event::SMSEvent, Event::Outbound, ACCOUNT2, group1.id());

    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1);
    addTestEvent(model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1);

    // status message:
    // NOTE: this event is not visible in any of the further tests
    addTestEvent(model, Event::StatusMessageEvent, Event::Outbound, ACCOUNT1,
                 group1.id(), "statue message", false, false,
                 QDateTime::currentDateTime(), QString(), true);

    QVERIFY(watcher.waitForAdded(13, 12));
}

void ConversationModelTest::getEvents_data()
{
    QTest::addColumn<bool>("useThread");

    QTest::newRow("Without thread") << false;
    QTest::newRow("Use thread") << true;
}

void ConversationModelTest::getEvents()
{
    QFETCH(bool, useThread);

    QThread modelThread;

    ConversationModel model;
    QSignalSpy modelReady(&model, &ConversationModel::modelReady);
    watcher.setModel(&model);

    if (useThread) {
        modelThread.start();
        model.setBackgroundThread(&modelThread);
    }

    QVERIFY(model.getEvents(group1.id()));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();

    QCOMPARE(model.rowCount(), 10);
    for (int i = 0; i < model.rowCount(); i++) {
        Event e1, e2;
        QModelIndex ind = model.index(i, 0);
        e1 = model.event(ind);
        QVERIFY(model.databaseIO().getEvent(e1.id(), e2));
        QVERIFY(compareEvents(e1, e2));
        QVERIFY(model.event(ind).type() != Event::CallEvent);
    }

    // add but don't save status message and check content again
    addTestEvent(model, Event::StatusMessageEvent, Event::Outbound, ACCOUNT1,
                 group1.id(), "status message", false, false,
                 QDateTime::currentDateTime(), QString(), true);
    QVERIFY(watcher.waitForAdded(1, 0));
    QCOMPARE(model.rowCount(), 11);
    for (int i = 0; i < model.rowCount(); i++)
        QVERIFY(model.event(model.index(i, 0)).type() != Event::CallEvent);
    // NOTE: since setFilter re-fetches data from tracker, status message event is lost

    /* filtering by type */
    QVERIFY(model.setFilter(Event::IMEvent));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();
    QCOMPARE(model.rowCount(), 6);
    for (int i = 0; i < model.rowCount(); i++)
        QCOMPARE(model.event(model.index(i, 0)).type(), Event::IMEvent);

    QVERIFY(model.setFilter(Event::SMSEvent));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();
    QCOMPARE(model.rowCount(), 4);
    for (int i = 0; i < model.rowCount(); i++)
        QCOMPARE(model.event(model.index(i, 0)).type(), Event::SMSEvent);

    /* filtering by account */
    QVERIFY(model.setFilter(Event::UnknownType, ACCOUNT1));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();
    QCOMPARE(model.rowCount(), 6);
    for (int i = 0; i < model.rowCount(); i++)
        QCOMPARE(model.event(model.index(i, 0)).localUid(), ACCOUNT1);

    QVERIFY(model.setFilter(Event::UnknownType, ACCOUNT2));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();
    QCOMPARE(model.rowCount(), 4);
    for (int i = 0; i < model.rowCount(); i++)
        QCOMPARE(model.event(model.index(i, 0)).localUid(), ACCOUNT2);

    /* filtering by direction */
    QVERIFY(model.setFilter(Event::UnknownType, QString(), Event::Inbound));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();
    QCOMPARE(model.rowCount(), 5);
    for (int i = 0; i < model.rowCount(); i++)
        QCOMPARE(model.event(model.index(i, 0)).direction(), Event::Inbound);

    QVERIFY(model.setFilter(Event::UnknownType, QString(), Event::Outbound));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();
    QCOMPARE(model.rowCount(), 5);
    for (int i = 0; i < model.rowCount(); i++)
        QCOMPARE(model.event(model.index(i, 0)).direction(), Event::Outbound);

    /* mixed filtering */
    QVERIFY(model.setFilter(Event::IMEvent, ACCOUNT1, Event::Outbound));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();
    QCOMPARE(model.rowCount(), 2);
    for (int i = 0; i < model.rowCount(); i++) {
        QCOMPARE(model.event(model.index(i, 0)).type(), Event::IMEvent);
        QCOMPARE(model.event(model.index(i, 0)).localUid(), ACCOUNT1);
        QCOMPARE(model.event(model.index(i, 0)).direction(), Event::Outbound);
    }

    /* without group ID filtering */
    ConversationModel allModel;
    QSignalSpy allModelReady(&allModel, &ConversationModel::modelReady);
    QVERIFY(allModel.getEvents());
    QTRY_COMPARE(allModelReady.count(), 1); allModelReady.clear();
    QCOMPARE(allModel.rowCount(), 12);

    QVERIFY(allModel.setFilter(Event::IMEvent));
    QTRY_COMPARE(allModelReady.count(), 1); allModelReady.clear();
    QCOMPARE(allModel.rowCount(), 6);

    QVERIFY(allModel.setFilter(Event::SMSEvent));
    QTRY_COMPARE(allModelReady.count(), 1); allModelReady.clear();
    QCOMPARE(allModel.rowCount(), 4);

    QVERIFY(allModel.setFilter(Event::CallEvent));
    QTRY_COMPARE(allModelReady.count(), 1); allModelReady.clear();
    QCOMPARE(allModel.rowCount(), 2);

    QVERIFY(allModel.setFilter(Event::UnknownType, ACCOUNT1));
    QTRY_COMPARE(allModelReady.count(), 1); allModelReady.clear();
    QCOMPARE(allModel.rowCount(), 8);

    QVERIFY(allModel.setFilter(Event::UnknownType, ACCOUNT2));
    QTRY_COMPARE(allModelReady.count(), 1); allModelReady.clear();
    QCOMPARE(allModel.rowCount(), 4);

    QVERIFY(allModel.setFilter(Event::UnknownType, QString(), Event::Inbound));
    QTRY_COMPARE(allModelReady.count(), 1); allModelReady.clear();
    QCOMPARE(allModel.rowCount(), 6);

    QVERIFY(allModel.setFilter(Event::UnknownType, QString(), Event::Outbound));
    QTRY_COMPARE(allModelReady.count(), 1); allModelReady.clear();
    QCOMPARE(allModel.rowCount(), 6);

    QVERIFY(allModel.setFilter(Event::IMEvent, ACCOUNT1, Event::Outbound));
    QTRY_COMPARE(allModelReady.count(), 1); allModelReady.clear();
    QCOMPARE(allModel.rowCount(), 2);

    modelThread.quit();
    modelThread.wait(3000);
}

void ConversationModelTest::addEvent()
{
    ConversationModel model;
    watcher.setModel(&model);
    model.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model.getEvents(group1.id()));
    int rows = model.rowCount();

    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1,
                         group1.id(), "added event") != -1);
    QVERIFY(watcher.waitForAdded());
    rows++;
    QCOMPARE(model.rowCount(), rows);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("added event"));

    /* filtering by type */
    QVERIFY(model.setFilter(Event::IMEvent));
    rows = model.rowCount();
    watcher.reset();
    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT1,
                         group1.id(), "im 1") != -1);
    QVERIFY(watcher.waitForAdded());
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                         group1.id(), "sms 1") != -1);
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), rows + 1);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("im 1"));

    /* filtering by account */
    QVERIFY(model.setFilter(Event::UnknownType, ACCOUNT1));
    rows = model.rowCount();
    watcher.reset();
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT2,
                         group1.id(), "account 2") != -1);
    QVERIFY(watcher.waitForAdded());
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                         group1.id(), "account 1") != -1);
    QVERIFY(watcher.waitForAdded());
    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1,
                         group1.id(), "account 1") != -1);
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), rows + 2);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("account 1"));

    /* filtering by direction */
    QVERIFY(model.setFilter(Event::UnknownType, "", Event::Inbound));
    rows = model.rowCount();
    watcher.reset();
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT2,
                         group1.id(), "in") != -1);
    QVERIFY(watcher.waitForAdded());
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Outbound, ACCOUNT1,
                         group1.id(), "out") != -1);
    QVERIFY(watcher.waitForAdded());
    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT1,
                         group1.id(), "in") != -1);
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), rows + 2);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("in"));

    /* mixed filtering */
    QVERIFY(model.setFilter(Event::SMSEvent, ACCOUNT2, Event::Inbound));
    rows = model.rowCount();
    watcher.reset();
    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT2,
                         group1.id(), "added event") != -1);
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), rows);
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT2,
                         group1.id(), "filtering works") != -1);
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), rows + 1);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("filtering works"));
}

void ConversationModelTest::modifyEvent()
{
    ConversationModel model;
    watcher.setModel(&model);
    model.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model.getEvents(group1.id()));

    Event event;
    /* modify invalid event */
    QVERIFY(!model.modifyEvent(event));

    QVERIFY(model.rowCount() > 0);

    int row = rand() % model.rowCount();
    event = model.event(model.index(row, 0));
    event.setFreeText("modified event");
    QDateTime modified = event.lastModified();
    watcher.reset();
    QVERIFY(model.modifyEvent(event));
    QVERIFY(watcher.waitForUpdated());
    QVERIFY(model.databaseIO().getEvent(event.id(), event));
    QCOMPARE(event.freeText(), QString("modified event"));

    event = model.event(model.index(row, 0));
    QCOMPARE(event.freeText(), QString("modified event"));
    QVERIFY(event.lastModified() > modified);
}

void ConversationModelTest::deleteEvent()
{
    ConversationModel model;
    watcher.setModel(&model);
    model.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model.getEvents(group1.id()));

    Event event;
    /* delete invalid event */
    QVERIFY(!model.deleteEvent(event));

    int rows = model.rowCount();
    int row = rand() % rows;
    event = model.event(model.index(row, 0));
    watcher.reset();
    QVERIFY(model.deleteEvent(event.id()));
    QVERIFY(watcher.waitForDeleted());
    QVERIFY(!model.databaseIO().getEvent(event.id(), event));
    QVERIFY(model.event(model.index(row, 0)).id() != event.id());
    QVERIFY(model.rowCount() == rows - 1);
}

void ConversationModelTest::asyncMode()
{
    ConversationModel model;
    QSignalSpy modelReady(&model, &ConversationModel::modelReady);
    QVERIFY(model.getEvents(group1.id()));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();
}

void ConversationModelTest::sorting()
{
    EventModel model;
    model.setQueryMode(EventModel::StreamedAsyncQuery);
    model.setFirstChunkSize(5);
    watcher.setModel(&model);

    //add events with the same timestamp
    QDateTime now = QDateTime::currentDateTime();
    QDateTime future = now.addSecs(10);

    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                 group1.id(), "I", false, false, now);
    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                 group1.id(), "II", false, false, now);
    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                 group1.id(), "III", false, false, future);
    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                 group1.id(), "IV", false, false, future);
    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                 group1.id(), "V", false, false, future);

    QVERIFY(watcher.waitForAdded(5));

    ConversationModel conv;
    conv.setQueryMode(EventModel::StreamedAsyncQuery);
    conv.setFirstChunkSize(5);
    QSignalSpy rowsInserted(&conv, SIGNAL(rowsInserted(const QModelIndex &, int, int)));

    QVERIFY(conv.getEvents(group1.id()));
    QTRY_COMPARE(rowsInserted.count(), 1);

    QVERIFY(conv.rowCount() >= 5 );

    QCOMPARE(conv.event(conv.index(0, 0)).freeText(), QLatin1String("V"));
    QCOMPARE(conv.event(conv.index(1, 0)).freeText(), QLatin1String("IV"));
    QCOMPARE(conv.event(conv.index(2, 0)).freeText(), QLatin1String("III"));
    QCOMPARE(conv.event(conv.index(3, 0)).freeText(), QLatin1String("II"));
    QCOMPARE(conv.event(conv.index(4, 0)).freeText(), QLatin1String("I"));
}

void ConversationModelTest::contacts_data()
{
    QTest::addColumn<QString>("localId");
    QTest::addColumn<QString>("remoteId");
    QTest::addColumn<int>("eventType");

    QTest::newRow("im") << "/org/freedesktop/Telepathy/Account/gabble/jabber/good_40localhost0"
            << "bad@cclocalhost"
            << (int)Event::IMEvent;
    QTest::newRow("cell") << RING_ACCOUNT
            << "+42992394"
            << (int)Event::SMSEvent;
}

void ConversationModelTest::contacts()
{
    QFETCH(QString, localId);
    QFETCH(QString, remoteId);
    QFETCH(int, eventType);

    ContactChangeListener contactChangeListener;

    Group group;
    addTestGroup(group, localId, remoteId);

    ConversationModel model;
    model.setResolveContacts(EventModel::ResolveImmediately);

    Event::PropertySet p;
    p.insert(Event::ContactId);
    p.insert(Event::ContactName);
    model.setPropertyMask(p);

    QSignalSpy modelReady(&model, &ConversationModel::modelReady);

    addTestEvent(model, (Event::EventType)eventType, Event::Inbound, localId,
                 group.id(), "text", false, false, QDateTime::currentDateTime(), remoteId);

    QVERIFY(model.getEvents(group.id()));
    QTRY_COMPARE(modelReady.count(), 1);
    modelReady.clear();

    Event event;
    event = model.event(model.index(0, 0));
    QCOMPARE(event.contactId(), 0);

    // Add a non-matching contact and check the contact does not resolve in the model.
    int contactId1 = addTestContact("Really1Funny", remoteId + "123", localId, &contactChangeListener);
    QVERIFY(model.getEvents(group.id()));
    QTRY_COMPARE(modelReady.count(), 1);
    modelReady.clear();
    event = model.event(model.index(0, 0));
    QCOMPARE(event.contactId(), 0);

    // Add a matching contact and check the contact is resolved in the model.
    int contactId = addTestContact("ReallyUFunny", remoteId, localId, &contactChangeListener);
    QVERIFY(model.getEvents(group.id()));
    QTRY_COMPARE(modelReady.count(), 1);
    modelReady.clear();
    QTRY_COMPARE(model.event(model.index(0, 0)).contactId(), contactId);
    QCOMPARE(model.event(model.index(0, 0)).contactName(), QString("ReallyUFunny"));

    deleteTestContact(contactId1, &contactChangeListener);
    deleteTestContact(contactId, &contactChangeListener);
}

void ConversationModelTest::reset()
{
    ConversationModel conv, allConv;
    QSignalSpy modelReady(&conv, &ConversationModel::modelReady);
    QSignalSpy allModelReady(&allConv, &ConversationModel::modelReady);

    QVERIFY(conv.getEvents(group1.id()));
    QTRY_COMPARE(modelReady.count(), 1);

    int group1Events = conv.rowCount();
    QVERIFY(group1Events >= 5 );

    QVERIFY(allConv.getEvents());
    QTRY_COMPARE(allModelReady.count(), 1);

    int allEvents = allConv.rowCount();
    QVERIFY(allEvents >= group1Events );

    QSignalSpy modelReset(&conv, SIGNAL(modelReset())), modelReset2(&allConv, SIGNAL(modelReset()));
    GroupModel groups;
    groups.deleteGroups(QList<int>() << group1.id());
    QTRY_COMPARE(modelReset.count(), 1);
    QCOMPARE(conv.rowCount(), 0);

    QTRY_COMPARE(modelReset2.count(), 1);
    QCOMPARE(allConv.rowCount(), (allEvents - group1Events));
}

void ConversationModelTest::cleanupTestCase()
{
    deleteAll();
}

QTEST_MAIN(ConversationModelTest)
