/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2020 D. Caliste.
** Contact: Damien Caliste <dcaliste@free.fr>
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

#include "recipienteventmodeltest.h"

#include "recipienteventmodel.h"
#include "event.h"
#include "common.h"
#include "databaseio.h"

#include <QtTest/QtTest>

Group group1, group2;

void RecipientEventModelTest::initTestCase()
{
    deleteAll();

    qsrand(QDateTime::currentDateTime().toTime_t());
}

void RecipientEventModelTest::cleanupTestCase()
{
    deleteAll();
    QTest::qWait(100);
}

void RecipientEventModelTest::testGetEvents_data()
{
    QTest::addColumn<QString>("localId");
    QTest::addColumn<QString>("remoteId");
    QTest::addColumn<QString>("readableRemoteId");
    QTest::addColumn<int>("eventType");

    QTest::newRow("im") << "/org/freedesktop/Telepathy/Account/gabble/jabber/good_40localhost0"
            << "abc@localhost"
            << "abc@localhost"
            << (int)Event::IMEvent;
    QTest::newRow("cell") << RING_ACCOUNT
            << "+42382394"
            << "+42 382 394"
            << (int)Event::SMSEvent;
}

void RecipientEventModelTest::testGetEvents()
{
    QFETCH(QString, localId);
    QFETCH(QString, remoteId);
    QFETCH(QString, readableRemoteId);
    QFETCH(int, eventType);

    deleteAll();
    QTest::qWait(100);

    addTestGroups(group1, group2);

    RecipientEventModel model;

    int eventId = addTestEvent(model, (Event::EventType)eventType, Event::Inbound, localId, group1.id(),
                               "text", false, false, QDateTime::currentDateTime(), remoteId, false);
    QVERIFY(eventId != -1);

    // Starting model is empty, nothing has been fetched
    QCOMPARE(model.rowCount(), 0);

    const Recipient testRecipient(localId, remoteId);
    QVERIFY(model.getEvents(testRecipient));
    QTRY_VERIFY(model.isReady());
    QCOMPARE(model.rowCount(), 1);

    Event event = model.event(model.index(0, 0));
    QCOMPARE(event.id(), eventId);
    QCOMPARE(event.recipients().size(), 1);
    QCOMPARE(event.recipients().first(), testRecipient);

    // Reset to an unused recipient
    QVERIFY(model.getEvents(Recipient(RING_ACCOUNT, "not-a-real-number")));
    QTRY_VERIFY(model.isReady());
    QCOMPARE(model.rowCount(), 0);

    // Add a non-matching event
    eventId = addTestEvent(model, (Event::EventType)eventType, Event::Inbound, localId, group1.id(),
                           "text", false, false, QDateTime::currentDateTime(), remoteId + "999", false);
    QVERIFY(eventId != -1);
    QCOMPARE(model.rowCount(), 0);

    // Still get the only the matching one.
    QVERIFY(model.getEvents(testRecipient));
    QTRY_VERIFY(model.isReady());
    QCOMPARE(model.rowCount(), 1);

    // Add another matching event, sometimes later
    eventId = addTestEvent(model, (Event::EventType)eventType, Event::Inbound, localId, group1.id(),
                           "text", false, false, QDateTime::currentDateTime().addSecs(1), remoteId, false);
    QVERIFY(eventId != -1);
    QCOMPARE(model.rowCount(), 2);
    // Sort is recent first.
    QCOMPARE(model.findEvent(eventId), model.index(0, 0));

    event = model.event(model.findEvent(eventId));
    QCOMPARE(event.id(), eventId);
    QCOMPARE(event.recipients().size(), 1);
    QCOMPARE(event.recipients().first(), testRecipient);
}

QTEST_MAIN(RecipientEventModelTest)
