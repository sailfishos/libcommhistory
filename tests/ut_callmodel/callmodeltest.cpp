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
#include "callmodeltest.h"
#include "commonutils.h"
#include "common.h"
#include "modelwatcher.h"
#include "databaseio.h"

using namespace CommHistory;

ModelWatcher watcher;

typedef QPair<int, QString> ContactDetails;

const QString REMOTEUID1( "user1@remotehost" );
const QString REMOTEUID2( "user2@remotehost" );

class TestCallItem
{
public:
    TestCallItem( const QString &remoteUid, CallEvent::CallType callType, int eventCount )
            : remoteUid( remoteUid )
            , callType( callType )
            , eventCount( eventCount )
        {};

    QString remoteUid;
    CallEvent::CallType callType;
    int eventCount;
};

QList<TestCallItem> testCalls;


void CallModelTest::initTestCase()
{
    initTestDatabase();
    QVERIFY( QDBusConnection::sessionBus().isConnected() );
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    srand( QDateTime::currentDateTime().toSecsSinceEpoch() );
#else
    qsrand( QDateTime::currentDateTime().toTime_t() );
#endif

    Group group1, group2;
    addTestGroups( group1, group2 );

    // add 8 call events from user1
    int cnt = 0;
    QDateTime when = QDateTime::currentDateTime();

    EventModel model;
    watcher.setModel(&model);
    // 2 dialed
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when,               REMOTEUID1 ); cnt++;
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs(  5 ), REMOTEUID1 ); cnt++;
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::DialedCallType, 2 ) );

    // 1 missed
    addTestEvent( model, Event::CallEvent, Event::Inbound,  ACCOUNT1, -1, "", false, true,  when.addSecs( 10 ), REMOTEUID1 ); cnt++;
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::MissedCallType, 1 ) );

    // 2 received
    addTestEvent( model, Event::CallEvent, Event::Inbound,  ACCOUNT1, -1, "", false, false, when.addSecs( 15 ), REMOTEUID1 ); cnt++;
    addTestEvent( model, Event::CallEvent, Event::Inbound,  ACCOUNT1, -1, "", false, false, when.addSecs( 20 ), REMOTEUID1 ); cnt++;
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::ReceivedCallType, 2 ) );

    // 1 dialed
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs( 25 ), REMOTEUID1 ); cnt++;
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::DialedCallType, 1 ) );

    // 2 missed
    addTestEvent( model, Event::CallEvent, Event::Inbound,  ACCOUNT1, -1, "", false, true,  when.addSecs( 30 ), REMOTEUID1 ); cnt++;
    addTestEvent( model, Event::CallEvent, Event::Inbound,  ACCOUNT1, -1, "", false, true,  when.addSecs( 35 ), REMOTEUID1 ); cnt++;
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::MissedCallType, 2 ) );

    // add 1 im and 2 sms events
    addTestEvent( model, Event::IMEvent,   Event::Outbound, ACCOUNT1, group1.id(), "test" );        cnt++;
    addTestEvent( model, Event::SMSEvent,  Event::Inbound,  ACCOUNT1, group1.id(), "test" );        cnt++;
    addTestEvent( model, Event::SMSEvent,  Event::Outbound, ACCOUNT1, group2.id(), "draft", true ); cnt++;

    QVERIFY(watcher.waitForAdded(cnt));
}

void CallModelTest::testGetEvents( CallModel::Sorting sorting, int row_count, QList<TestCallItem> calls )
{
    CallModel model;
    model.setQueryMode(EventModel::SyncQuery);

    model.setFilter(  sorting  );
    QVERIFY( model.getEvents() );

    QCOMPARE( model.rowCount(), row_count );

    QSet<QString> countedUids;
    for ( int i = 0; i < row_count; i++ )
    {
        Event e = model.event( model.index( i, 0 ) );
        QCOMPARE( e.type(), Event::CallEvent );
        QCOMPARE( e.recipients().value(0).remoteUid(), calls.at( i ).remoteUid );

        bool addressFound = false;
        foreach (QString address, countedUids) {
            if (address == e.recipients().value(0).remoteUid()) {
                addressFound = true;
                break;
            }
        }

        switch ( calls.at( i ).callType )
        {
            case CallEvent::MissedCallType :
            {
                QCOMPARE( e.direction(), Event::Inbound );
                QCOMPARE( e.isMissedCall(), true );
                if (addressFound)
                    QVERIFY(e.eventCount() <= 1);
                else
                    QCOMPARE( e.eventCount(), calls.at( i ).eventCount );
                break;
            }
            case CallEvent::ReceivedCallType :
            {
                QCOMPARE( e.direction(), Event::Inbound );
                QCOMPARE( e.isMissedCall(), false );
                // received and missed calls have invalid event count if sorted by contact
                if (addressFound)
                    QVERIFY(e.eventCount() <= 1);
                else
                    QCOMPARE( e.eventCount(), sorting == CallModel::SortByContact ? 0 : calls.at( i ).eventCount );
                break;
            }
            case CallEvent::DialedCallType :
            {
                QCOMPARE( e.direction(), Event::Outbound );
                QCOMPARE( e.isMissedCall(), false );
                // received and missed calls have invalid event count if sorted by contact
                if (addressFound)
                    QVERIFY(e.eventCount() <= 1);
                else
                    QCOMPARE( e.eventCount(), sorting == CallModel::SortByContact ? 0 : calls.at( i ).eventCount );
                break;
            }
            default :
            {
                qCritical() << "Unknown call type!";
                return;
            }
        }

        countedUids.insert(e.recipients().value(0).remoteUid());
    }
}

void CallModelTest::testAddEvent()
{
    CallModel model;
    watcher.setModel(&model);
    model.setQueryMode( EventModel::SyncQuery );

    /* by contact:
     * -----------
     * user1, missed   (2)
     */
    testGetEvents( CallModel::SortByContact, 1, testCalls );
    /* by time:
     * --------
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );

    // add 1 dialed from user1
    QDateTime when = QDateTime::currentDateTime();
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs( 40 ), REMOTEUID1 );
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::DialedCallType, 1 ) );
    QVERIFY(watcher.waitForAdded());

    /* by contact:
     * -----------
     * user1, dialed   (-1)
     */
    testGetEvents( CallModel::SortByContact, 1, testCalls );
    /* by time:
     * --------
     * user1, dialed   (1)
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );

    // add 5 received from user1
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs( 45 ), REMOTEUID1 );
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs( 50 ), REMOTEUID1 );
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs( 55 ), REMOTEUID1 );
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs( 60 ), REMOTEUID1 );
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs( 65 ), REMOTEUID1 );
    QVERIFY(watcher.waitForAdded(5));
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::ReceivedCallType, 5 ) );

    /* by contact:
     * -----------
     * user1, received (-1)
     */
    testGetEvents( CallModel::SortByContact, 1, testCalls );
    /* by time:
     * --------
     * user1, received (5)
     * user1, dialed   (1)
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );

    // add 1 missed from user2
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when.addSecs( 70 ), REMOTEUID2 );
    testCalls.insert( 0, TestCallItem( REMOTEUID2, CallEvent::MissedCallType, 1 ) );
    QVERIFY(watcher.waitForAdded());

    /* by contact:
     * -----------
     * user2, missed   (1)
     * user1, received (0)
     */
    testGetEvents( CallModel::SortByContact, 2, testCalls );
    /* by time:
     * --------
     * user2, received (1)
     * user1, received (5)
     * user1, dialed   (1)
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );

    // add 1 received from user1
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs( 75 ), REMOTEUID1 );
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::ReceivedCallType, 1 ) );
    QVERIFY(watcher.waitForAdded());

    /* by contact: ***REORDERING
     * -----------
     * user1, received (0)
     * user2, missed   (1)
     */
    testGetEvents( CallModel::SortByContact, 2, testCalls );
    /* by time:
     * --------
     * user1, received (1)
     * user2, missed   (1)
     * user1, received (5)
     * user1, dialed   (1)
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );

    // add 1 received from user2
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs( 80 ), REMOTEUID2 );
    testCalls.insert( 0, TestCallItem( REMOTEUID2, CallEvent::ReceivedCallType, 1 ) );
    QVERIFY(watcher.waitForAdded());

    /* by contact:
     * -----------
     * user2, received (0)
     * user1, received (0)
     */
    testGetEvents( CallModel::SortByContact, 2, testCalls );
    /* by time:
     * --------
     * user2, received (1)
     * user1, received (1)
     * user2, missed   (1)
     * user1, received (5)
     * user1, dialed   (1)
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );

    // add 1 received from hidden number
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs(90), "<hidden>");
    testCalls.insert(0, TestCallItem("<hidden>", CallEvent::ReceivedCallType, 1));
    QVERIFY(watcher.waitForAdded());

    /* by contact:
     * -----------
     * "", received (0)
     * user2, received (0)
     * user1, received (0)
     */
    testGetEvents( CallModel::SortByContact, 3, testCalls );
    /* by time:
     * --------
     * "", received (1)
     * user2, received (1)
     * user1, received (1)
     * user2, missed   (1)
     * user1, received (5)
     * user1, dialed   (1)
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );
}

void CallModelTest::testDeleteEvent()
{
    CallModel model;
    QSignalSpy modelReady(&model, &CallModel::modelReady);
    watcher.setModel(&model);

    model.setTreeMode(false);
    QVERIFY(model.getEvents());
    QTRY_COMPARE(modelReady.count(), 1);

    // delete first event from hidden number
    Event e = model.event(model.index(0, 0));
    QVERIFY(e.isValid());
    QCOMPARE(e.recipients().value(0).remoteUid(), QLatin1String("<hidden>"));
    QVERIFY(model.deleteEvent(e.id()));
    QVERIFY(watcher.waitForDeleted());
    // correct test helper lists to match current situation
    QMutableListIterator<TestCallItem> i(testCalls);
    while (i.hasNext()) {
        i.next();
        if (i.value().remoteUid == QLatin1String("<hidden>"))
            i.remove();
    }

    // force change of sorting to SortByContact
    modelReady.clear();
    model.setTreeMode(true);
    QVERIFY( model.setFilter( CallModel::SortByContact ) );
    QVERIFY( model.getEvents() );
    QTRY_COMPARE(modelReady.count(), 2);  // setFilter() internally called getEvents(), triggers an additional modelReady()

    /* by contact:
     * -----------
     * user2, received (0)
     * user1, received (0)
     */
    // delete first group from user2
    e = model.event( model.index( 0, 0 ) );
    QVERIFY( e.isValid() );
    QCOMPARE( e.type(), Event::CallEvent );
    QCOMPARE( e.direction(), Event::Inbound );
    QCOMPARE( e.isMissedCall(), false );
    QCOMPARE( e.recipients().value(0).remoteUid(), REMOTEUID2 );
    // delete it
    QVERIFY( model.deleteEvent( e.id() ) );
    QVERIFY( watcher.waitForDeleted(2) );
    // correct test helper lists to match current situation
    i = QMutableListIterator<TestCallItem>(testCalls);
    while (i.hasNext()) {
        i.next();
        if (i.value().remoteUid == REMOTEUID2)
            i.remove();
    }
    // test if model contains what we want it does
    testGetEvents( CallModel::SortByContact, 1, testCalls );

    // force change of sorting to SortByTime
    modelReady.clear();
    QVERIFY( model.setFilter( CallModel::SortByTime ) );
    QTRY_COMPARE(modelReady.count(), 1);

    /* by time:
     * --------
     * user1, received (6)***
     * user1, dialed   (1)
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     *
     *     ||
     *     \/
     *
     * user1, dialed   (1)
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    // take the event
    e = model.event( model.index( 0, 0 ) );
    QVERIFY( e.isValid() );
    QCOMPARE( e.type(), Event::CallEvent );
    QCOMPARE( e.direction(), Event::Inbound );
    QCOMPARE( e.isMissedCall(), false );
    // delete it
    QVERIFY( model.deleteEvent( e.id() ) );
    QVERIFY( watcher.waitForDeleted(6) );
    // correct test helper lists to match current situation
    testCalls.takeFirst(); testCalls.takeFirst();
    // test if model contains what we want it does
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );

    /* by time:
     * --------
     * user1, dialed   (1) <---\
     * user1, missed   (2)***   regrouping
     * user1, dialed   (1) <---/
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     *
     *     ||
     *     \/
     *
     * user1, dialed   (2)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    // take the event
    e = model.event( model.index( 1, 0 ) );
    QVERIFY( e.isValid() );
    QCOMPARE( e.type(), Event::CallEvent );
    QCOMPARE( e.direction(), Event::Inbound );
    QCOMPARE( e.isMissedCall(), true );
    // delete it
    QVERIFY( model.deleteEvent( e.id() ) );
    QVERIFY( watcher.waitForDeleted(2) );
    // correct test helper lists to match current situation
    testCalls.takeFirst(); testCalls.takeFirst(); testCalls.first().eventCount = 2;
    // test if model contains what we want it does
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );


    // force change of sorting to SortByContact
    modelReady.clear();
    QVERIFY( model.setFilter( CallModel::SortByContact ) );
    QTRY_COMPARE(modelReady.count(), 1);
    /* by contact:
     * -----------
     * user1, dialed (0)***
     *
     *    ||
     *    \/
     *
     * (empty)
     */
    // take the event
    e = model.event( model.index( 0, 0 ) );
    QVERIFY( e.isValid() );
    QCOMPARE( e.type(), Event::CallEvent );
    QCOMPARE( e.direction(), Event::Outbound );
    QCOMPARE( e.isMissedCall(), false );
    // delete it
    QVERIFY( model.deleteEvent( e.id() ) );
    QVERIFY( watcher.waitForDeleted(7) );
    // correct test helper lists to match current situation
    testCalls.clear();
    // test if model contains what we want it does
    testGetEvents( CallModel::SortByContact, 0, testCalls );
}

void CallModelTest::testGetEventsTimeTypeFilter_data()
{
    QTest::addColumn<bool>("useThread");

    QTest::newRow("Without thread") << false;
    QTest::newRow("Use thread") << true;
}

void CallModelTest::testGetEventsTimeTypeFilter()
{
    QFETCH(bool, useThread);

    deleteAll(false);

    QThread modelThread;

    //initTestCase ==> 3 dialled calls, 2 Received calls, 3 Missed Calls already added
    CallModel model;
    QSignalSpy modelReady(&model, &CallModel::modelReady);
    watcher.setModel(&model);
    if (useThread) {
        modelThread.start();
        model.setBackgroundThread(&modelThread);
    }

    QDateTime when = QDateTime::currentDateTime();
    model.setTreeMode(false);

    modelReady.clear();
    QVERIFY(model.setFilter(CallModel::SortByTime,  CallEvent::DialedCallType, when));
    QVERIFY(model.getEvents());
    QTRY_COMPARE(modelReady.count(), 1);
    int outboundCallCount = model.rowCount();

    modelReady.clear();
    QVERIFY(model.setFilter(CallModel::SortByTime,  CallEvent::MissedCallType, when));
    QVERIFY(model.getEvents());
    QTRY_COMPARE(modelReady.count(), 2);  // setFilter() internally called getEvents(), triggers an additional modelReady()
    int missedCallCount = model.rowCount();

    modelReady.clear();
    QVERIFY(model.setFilter(CallModel::SortByTime,  CallEvent::ReceivedCallType, when));
    QVERIFY(model.getEvents());
    QTRY_COMPARE(modelReady.count(), 2);
    int receivedCallCount = model.rowCount();

    //3 dialled
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when );
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs(5) );
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs(10) );
    QVERIFY(watcher.waitForAdded(3));

    //2 received
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when );
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs(5) );
    QVERIFY(watcher.waitForAdded(2));

    //3 missed
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when );
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when.addSecs(5) );
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when.addSecs(10) );
    QVERIFY(watcher.waitForAdded(3));

    QDateTime time = when;
    //model.setQueryMode(EventModel::SyncQuery);
    modelReady.clear();
    QVERIFY(model.setFilter(CallModel::SortByTime,  CallEvent::DialedCallType, time));
    QVERIFY(model.getEvents());
    QTRY_COMPARE(modelReady.count(), 2);

    QCOMPARE(model.rowCount(), outboundCallCount + 3);
    Event e1 = model.event(model.index(0,0));
    Event e2 = model.event(model.index(1,0));
    Event e3 = model.event(model.index(2,0));
    QVERIFY(e1.isValid());
    QVERIFY(e2.isValid());
    QVERIFY(e3.isValid());
    QVERIFY(e1.direction() == Event::Outbound);
    QVERIFY(e2.direction() == Event::Outbound);
    QVERIFY(e3.direction() == Event::Outbound);

    modelReady.clear();
    QVERIFY(model.setFilter(CallModel::SortByTime, CallEvent::MissedCallType, time));
    QTRY_COMPARE(modelReady.count(), 1);
    QCOMPARE(model.rowCount(), missedCallCount + 3);
    modelReady.clear();
    QVERIFY(model.setFilter(CallModel::SortByTime, CallEvent::ReceivedCallType, time));
    QTRY_COMPARE(modelReady.count(), 1);
    QCOMPARE(model.rowCount(), receivedCallCount + 2);

    /**
      * testing to check for adding events with wrong filters
      */
    time = when.addSecs(-60*5);
    //adding one more received but 5 minutes before the set time filter
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, time );
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), receivedCallCount + 2); //event should not be added to model, so rowCount should remain same for received calls
    //filter is set for received call, try to add missed and dialled calls with correct time filter
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when );
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), receivedCallCount + 2); //event should not be added to model, so rowCount should remain same which was for received calls
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when );
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), receivedCallCount + 2); //event should not be added to model, so rowCount should remain same which was for received calls

    /**
      ** testing to check for getting events after he time when all events addition was complete
      */
    //Trying to get events after 5 minutes after the  first event was added
    time = when.addSecs(60*5);
    modelReady.clear();
    QVERIFY(model.setFilter(CallModel::SortByTime, CallEvent::ReceivedCallType, time));
    QTRY_COMPARE(modelReady.count(), 1);
    QVERIFY(model.rowCount() == 0);

    modelThread.quit();
    modelThread.wait(3000);
}

void CallModelTest::testSortByContactUpdate()
{
    deleteAll(false);

    CallModel model;
    QSignalSpy modelReady(&model, &CallModel::modelReady);
    watcher.setModel(&model);

    /*
     * user1, missed   (2)
     * (user1, dialed)
     * user1, missed   (1)
     */
    QDateTime when = QDateTime::currentDateTime();
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when, REMOTEUID1);
    addTestEvent(model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs(1), REMOTEUID1);
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when.addSecs(2), REMOTEUID1);
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when.addSecs(3), REMOTEUID1);
    QVERIFY(watcher.waitForAdded(4));

    modelReady.clear();
    QVERIFY(model.setFilter(CallModel::SortByContact));
    QVERIFY(model.getEvents());
    QTRY_COMPARE(modelReady.count(), 1);
    QCOMPARE(model.rowCount(), 1);

    Event e1 = model.event(model.index(0, 0));
    QCOMPARE(e1.recipients().value(0).remoteUid(), REMOTEUID1);
    QVERIFY(e1.isMissedCall());
    QCOMPARE(e1.eventCount(), 2);

    // add received call
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs(4), REMOTEUID1);
    QVERIFY(watcher.waitForAdded());
    e1 = model.event(model.index(0, 0));
    QCOMPARE(e1.eventCount(), 1);
    QCOMPARE(e1.direction(), Event::Inbound);

    // add call to another contact
    addTestEvent(model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs(5), REMOTEUID2);
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), 2);
    e1 = model.event(model.index(0, 0));
    QCOMPARE(e1.eventCount(), 1);
    QCOMPARE(e1.direction(), Event::Outbound);
    QCOMPARE(e1.recipients().value(0).remoteUid(), REMOTEUID2);

    // missed call to first contact, should reorder
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when.addSecs(6), REMOTEUID1);
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), 2);
    e1 = model.event(model.index(0, 0));
    int firstMissedId = e1.id();
    QVERIFY(e1.isMissedCall());
    QCOMPARE(e1.recipients().value(0).remoteUid(), REMOTEUID1);
    QCOMPARE(e1.eventCount(), 1);

    // another missed call, increase event count
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when.addSecs(7), REMOTEUID1);
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), 2);
    e1 = model.event(model.index(0, 0));
    QVERIFY(e1.isMissedCall());
    QCOMPARE(e1.recipients().value(0).remoteUid(), REMOTEUID1);
    QCOMPARE(e1.eventCount(), 2);

    // mark latest missed call as read, the first should also be marked
    Event e = model.event(model.index(0, 0));
    e.setIsRead(true);
    QVERIFY(model.modifyEvent(e));
    QVERIFY(watcher.waitForUpdated(7));
    QVERIFY(model.databaseIO().getEvent(firstMissedId, e));
    QVERIFY(e.isRead());

    // add call to the other contact...
    addTestEvent(model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs(10), REMOTEUID2);
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), 2);
    e1 = model.event(model.index(0, 0));
    QCOMPARE(e1.eventCount(), 2);
    QCOMPARE(e1.direction(), Event::Outbound);
    QCOMPARE(e1.recipients().value(0).remoteUid(), REMOTEUID2);

    // ...and a missed call to the first -> move to top and increase event count
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when.addSecs(15), REMOTEUID1);
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), 2);
    e1 = model.event(model.index(0, 0));
    QVERIFY(e1.isMissedCall());
    QCOMPARE(e1.recipients().value(0).remoteUid(), REMOTEUID1);
    QCOMPARE(e1.eventCount(), 3);
}

void CallModelTest::testSortByTimeUpdate()
{
    deleteAll(false);

    CallModel model;
    QSignalSpy modelReady(&model, &CallModel::modelReady);
    watcher.setModel(&model);

    /*
     * user1, missed   (2)
     * (user1, dialed)
     * user1, missed   (1)
     */
    QDateTime when = QDateTime::currentDateTime();
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when, REMOTEUID2);
    addTestEvent(model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs(1), REMOTEUID1);
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when.addSecs(2), REMOTEUID1);
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when.addSecs(3), REMOTEUID1);
    QVERIFY(watcher.waitForAdded(4));

    modelReady.clear();
    QVERIFY(model.setFilter(CallModel::SortByTime, CallEvent::MissedCallType));
    QVERIFY(model.getEvents());
    QTRY_COMPARE(modelReady.count(), 1);
    QCOMPARE(model.rowCount(), 2);

    Event e1 = model.event(model.index(0, 0));
    QCOMPARE(e1.recipients().value(0).remoteUid(), REMOTEUID1);
    QVERIFY(e1.isMissedCall());
    QCOMPARE(e1.eventCount(), 2);

    Event e2 = model.event(model.index(1, 0));
    QCOMPARE(e2.recipients().value(0).remoteUid(), REMOTEUID2);
    QVERIFY(e2.isMissedCall());
    QVERIFY(e2.eventCount() <= 1);

    // add received call, should be filtered out
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs(4), REMOTEUID1);
    QVERIFY(watcher.waitForAdded());
    e1 = model.event(model.index(0, 0));
    QVERIFY(e1.isMissedCall());
    QCOMPARE(e1.eventCount(), 2);

    CallModel model2;
    model2.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model2.getEvents(CallModel::SortByTime, CallEvent::MissedCallType));

    // add missed call, count should increase to 3
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when.addSecs(5), REMOTEUID1);
    QVERIFY(watcher.waitForAdded());
    e1 = model.event(model.index(0, 0));
    int firstMissedId = e1.id();
    QCOMPARE(e1.eventCount(), 3);

    QVERIFY(model2.getEvents(CallModel::SortByTime, CallEvent::MissedCallType));
    QCOMPARE(model2.rowCount(), 2);
    QCOMPARE(model2.event(model2.index(0, 0)).eventCount(), 3);

    // mark latest missed call as read, the first should also be marked
    Event e = model.event(model.index(0, 0));
    e.setIsRead(true);
    QVERIFY(model.modifyEvent(e));
    QVERIFY(watcher.waitForUpdated(3));
    QVERIFY(model.databaseIO().getEvent(firstMissedId, e));
    QVERIFY(e.isRead());
}

void CallModelTest::testSIPAddress()
{
    QSKIP("SIP address resolution is not currently supported");
    ContactChangeListener contactChangeListener;

    CallModel model;
    model.setResolveContacts(EventModel::ResolveImmediately);
    watcher.setModel(&model);

    QString account(RING_ACCOUNT);
    QString contactName("Donkey Kong");
    QString phoneNumber("012345678");
    QString sipAddress("sips:012345678@voip.com");

    QString contactName2("Mario");
    QString sipAddress2("sips:mario@voip.com");

    QDateTime when = QDateTime::currentDateTime();
    int contactId = addTestContact(contactName, phoneNumber, account, &contactChangeListener);
    QVERIFY(contactId != -1);
    int contactId2 = addTestContact(contactName2, sipAddress2, account, &contactChangeListener);
    QVERIFY(contactId2 != -1);

    // normal phone call
    addTestEvent(model, Event::CallEvent, Event::Outbound, account, -1, "", false, false, when, phoneNumber);
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), 1);
    Event e = model.event(model.index(0, 0));
    QCOMPARE(e.recipients().value(0).remoteUid(), phoneNumber);

    // SIP call to same number, should group with first event
    addTestEvent(model, Event::CallEvent, Event::Outbound, account, -1, "", false, false, when.addSecs(5), sipAddress);
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), 1);
    e = model.event(model.index(0, 0));
    QCOMPARE(e.recipients().value(0).remoteUid(), sipAddress);

    // SIP call to non-numeric address
    addTestEvent(model, Event::CallEvent, Event::Outbound, account, -1, "", false, false, when.addSecs(10), sipAddress2);
    QVERIFY(watcher.waitForAdded());
    QCOMPARE(model.rowCount(), 2);
    e = model.event(model.index(0, 0));
    QCOMPARE(e.recipients().value(0).remoteUid(), sipAddress2);

    // check contact resolving for call groups
    QVERIFY(model.setFilter(CallModel::SortByContact));
    QVERIFY(model.getEvents());
    QCOMPARE(model.rowCount(), 2);
    e = model.event(model.index(0, 0));
    QCOMPARE(e.recipients().value(0).remoteUid(), sipAddress2);
    QVERIFY(!e.contacts().isEmpty());
    QCOMPARE(e.contacts().first().first, contactId2);
    QCOMPARE(e.contacts().first().second, contactName2);

    e = model.event(model.index(1, 0));
    QCOMPARE(e.recipients().value(0).remoteUid(), sipAddress);
    QVERIFY(!e.contacts().isEmpty());
    QCOMPARE(e.contacts().first().first, contactId);
    QCOMPARE(e.contacts().first().second, contactName);

    // check contact resolving when sorting by time
    QVERIFY(model.setFilter(CallModel::SortByTime));
    QVERIFY(model.getEvents());
    QCOMPARE(model.rowCount(), 2);

    e = model.event(model.index(0, 0));
    QCOMPARE(e.recipients().value(0).remoteUid(), sipAddress2);
    QVERIFY(!e.contacts().isEmpty());
    QCOMPARE(e.contacts().first().first, contactId2);
    QCOMPARE(e.contacts().first().second, contactName2);

    e = model.event(model.index(1, 0));
    QCOMPARE(e.recipients().value(0).remoteUid(), sipAddress);
    QVERIFY(!e.contacts().isEmpty());
    QCOMPARE(e.contacts().first().first, contactId);
    QCOMPARE(e.contacts().first().second, contactName);

    deleteTestContact(contactId, &contactChangeListener);
    deleteTestContact(contactId2, &contactChangeListener);
}

void CallModelTest::deleteAllCalls()
{
    CallModel model;
    watcher.setModel(&model);
    model.setQueryMode(EventModel::SyncQuery);
    QDateTime when = QDateTime::currentDateTime();
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs( 54 ), REMOTEUID1 );
    QVERIFY(watcher.waitForAdded());

    QVERIFY(model.getEvents());
    QVERIFY(model.rowCount() > 0);
    QSignalSpy eventsCommitted(&model, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    QVERIFY(model.deleteAll());
    QTRY_COMPARE(eventsCommitted.count(), 1);

    QCOMPARE(model.rowCount(), 0);

    QVERIFY(model.getEvents());
    QCOMPARE(model.rowCount(), 0);
}

void CallModelTest::testMarkAllRead()
{
    CallModel callModel;

    QSignalSpy eventsCommitted(&callModel, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&,
                                                                   bool)));
    // Test event is unread by default.
    int eventId1 = addTestEvent(callModel, Event::CallEvent, Event::Inbound, ACCOUNT1, -1,
                                "Mark all as read test 1", false, false,
                                QDateTime::currentDateTime(), REMOTEUID1);
    QTRY_COMPARE(eventsCommitted.count(), 1);

    eventsCommitted.clear();
    int eventId2 = addTestEvent(callModel, Event::CallEvent, Event::Inbound, ACCOUNT1, -1,
                                "Mark all as read test 2", false, false,
                                QDateTime::currentDateTime(), REMOTEUID2);
    QTRY_COMPARE(eventsCommitted.count(), 1);

    eventsCommitted.clear();
    QVERIFY(callModel.markAllRead());
    QTRY_COMPARE(eventsCommitted.count(), 1);

    Event e1, e2;
    QVERIFY(callModel.databaseIO().getEvent(eventId1, e1));
    QVERIFY(e1.isRead());

    QVERIFY(callModel.databaseIO().getEvent(eventId2, e2));
    QVERIFY(e2.isRead());
}

void CallModelTest::testLimit()
{
    CallModel model;
    model.setQueryMode(EventModel::SyncQuery);
    model.setFilter(CallModel::SortByTime);

    QVERIFY(model.getEvents());
    QVERIFY(model.rowCount() > 1);

    model.setLimit(1);

    QVERIFY(model.getEvents());

    QCOMPARE(model.rowCount(), 1);
    Event e1 = model.event(model.index(0, 0));

    model.setOffset(1);

    QVERIFY(model.getEvents());

    QCOMPARE(model.rowCount(), 1);
    Event e2 = model.event(model.index(0, 0));

    QVERIFY(e1.id() != e2.id());
}

void CallModelTest::testModifyEvent()
{
    Event e1, e2, e3;

    deleteAll(false);
    QTest::qWait(100);

    CallModel model;
    QSignalSpy modelReady(&model, &CallModel::modelReady);
    watcher.setModel(&model);

    /*
     * user1, received
     * user1, dialed
     * user2, missed
     * user1, received
     * -> displayed as
     * user1, received
     * user2, missed
     */
    QDateTime when = QDateTime::currentDateTime();
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs(1), REMOTEUID1);
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when.addSecs(2), REMOTEUID2);
    addTestEvent(model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs(3), REMOTEUID1);
    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs(4), REMOTEUID1);
    QVERIFY(watcher.waitForAdded(4));

    modelReady.clear();
    QVERIFY(model.setFilter(CallModel::SortByContact));
    QVERIFY(model.getEvents());
    QTRY_COMPARE(modelReady.count(), 1);
    QCOMPARE(model.rowCount(), 2);
    e1 = model.event(model.index(0, 0));
    QCOMPARE(e1.recipients().value(0).remoteUid(), REMOTEUID1);
    QCOMPARE(e1.direction(), Event::Inbound);
    e2 = model.event(model.index(1, 0));
    QCOMPARE(e2.recipients().value(0).remoteUid(), REMOTEUID2);
    QCOMPARE(e2.direction(), Event::Inbound);

    /*
     * upgrade latest user1 call to video:
     * user1, received, video
     * user1, dialed
     * user2, missed
     */
    e1.setIsVideoCall(true);
    QVERIFY(model.modifyEvent(e1));
    QVERIFY(watcher.waitForUpdated());

    e1 = model.event(model.index(0, 0));
    QCOMPARE(e1.recipients().value(0).remoteUid(), REMOTEUID1);
    QCOMPARE(e1.direction(), Event::Inbound);
    QCOMPARE(e1.isVideoCall(), true);
    e2 = model.event(model.index(1, 0));
    QCOMPARE(e2.recipients().value(0).remoteUid(), REMOTEUID1);
    QCOMPARE(e2.direction(), Event::Outbound);
    QCOMPARE(e2.isVideoCall(), false);
    e3 = model.event(model.index(2, 0));
    QCOMPARE(e3.recipients().value(0).remoteUid(), REMOTEUID2);
    QCOMPARE(e3.direction(), Event::Inbound);
    QCOMPARE(e3.isVideoCall(), false);

    /*
     * downgrade back to audio:
     * user1, received
     * user2, missed
     */
    e1.setIsVideoCall(false);
    QVERIFY(model.modifyEvent(e1));
    QVERIFY(watcher.waitForUpdated());

    e1 = model.event(model.index(0, 0));
    QCOMPARE(e1.recipients().value(0).remoteUid(), REMOTEUID1);
    QCOMPARE(e1.direction(), Event::Inbound);
    e2 = model.event(model.index(1, 0));
    QCOMPARE(e2.recipients().value(0).remoteUid(), REMOTEUID2);
    QCOMPARE(e2.direction(), Event::Inbound);
}

// Test that phone numbers resolve to the same contact if they minimize
// to the same number.
void CallModelTest::testMinimizedPhone()
{
    deleteAll(false);

    ContactChangeListener contactChangeListener;

    const QString phone00("0011112222");
    const QString phone99("9911112222");
    // Precondition for the test:
    QCOMPARE(minimizePhoneNumber(phone00), minimizePhoneNumber(phone99)); // enum { DefaultMaximumPhoneNumberCharacters = 8 }

    const QString user00("User00");
    const QString user99("User99");

    int user00id = addTestContact(user00, phone00, RING_ACCOUNT, &contactChangeListener);
    int user99id = addTestContact(user99, phone99, RING_ACCOUNT, &contactChangeListener);

    CallModel model;
    QSignalSpy modelReady(&model, &CallModel::modelReady);
    watcher.setModel(&model);

    QDateTime when = QDateTime::currentDateTime();
    addTestEvent(model, Event::CallEvent, Event::Inbound, RING_ACCOUNT, -1, "", false, false, when, phone00);
    addTestEvent(model, Event::CallEvent, Event::Inbound, RING_ACCOUNT, -1, "", false, true, when.addSecs(10), phone99);
    addTestEvent(model, Event::CallEvent, Event::Outbound, RING_ACCOUNT, -1, "", false, false, when.addSecs(20), phone00);
    QVERIFY(watcher.waitForAdded(3));

    modelReady.clear();
    model.setFilter(CallModel::SortByTime);
    model.setResolveContacts(EventModel::ResolveImmediately);
    QVERIFY(model.getEvents());
    QTRY_COMPARE(modelReady.count(), 1);
    QCOMPARE(model.rowCount(), 3);

    for (int i = 0; i < model.rowCount(); ++i) {
        Event e = model.event(model.index(i, 0));
        QCOMPARE(e.recipients().count(), 1);
        QCOMPARE(e.recipients().at(0), Recipient::fromPhoneNumber(phone00));
        QCOMPARE(e.recipients().at(0).isContactResolved(), true);
        QCOMPARE(e.recipients().at(0).contactId(), user00id);
        QCOMPARE(e.recipients().at(0).contactName(), user00);
    }

    // If we delete one of these contacts the number should resolve to the other
    deleteTestContact(user00id, &contactChangeListener);

    QCOMPARE(model.rowCount(), 3);
    for (int i = 0; i < model.rowCount(); ++i) {
        Event e = model.event(model.index(i, 0));
        QCOMPARE(e.recipients().count(), 1);
        QCOMPARE(e.recipients().at(0), Recipient::fromPhoneNumber(phone99));
        QTRY_COMPARE(e.recipients().at(0).contactId(), user99id);
        QCOMPARE(e.recipients().at(0).contactName(), user99);
    }

    // Delete the other contact, the numbers should no longer resolve to any contact
    deleteTestContact(user99id, &contactChangeListener);

    QCOMPARE(model.rowCount(), 3);
    for (int i = 0; i < model.rowCount(); ++i) {
        Event e = model.event(model.index(i, 0));
        QCOMPARE(e.recipients().count(), 1);
        QCOMPARE(e.recipients().at(0), Recipient::fromPhoneNumber(phone99));
        QTRY_COMPARE(e.recipients().at(0).contactId(), 0);
        QCOMPARE(e.recipients().at(0).contactName(), QString());
    }
}

// Ensure that non-numeric phone numbers are not coalesced together
void CallModelTest::testMinimizedEmpty()
{
    deleteAll(false);

    const QString phone1("The Palace");
    const QString phone2("The Police");
    // Precondition for the test:
    QCOMPARE(minimizePhoneNumber(phone1), QString());
    QCOMPARE(minimizePhoneNumber(phone2), QString());

    CallModel model;
    QSignalSpy modelReady(&model, &CallModel::modelReady);
    watcher.setModel(&model);

    QDateTime when = QDateTime::currentDateTime();
    addTestEvent(model, Event::CallEvent, Event::Inbound, RING_ACCOUNT, -1, "", false, false, when, phone1);
    addTestEvent(model, Event::CallEvent, Event::Inbound, RING_ACCOUNT, -1, "", false, false, when.addSecs(10), phone1);
    addTestEvent(model, Event::CallEvent, Event::Inbound, RING_ACCOUNT, -1, "", false, false, when.addSecs(20), phone2);
    addTestEvent(model, Event::CallEvent, Event::Inbound, RING_ACCOUNT, -1, "", false, false, when.addSecs(30), phone2);
    QVERIFY(watcher.waitForAdded(4));

    modelReady.clear();
    model.setFilter(CallModel::SortByTime);
    model.setResolveContacts(EventModel::ResolveImmediately);
    QVERIFY(model.getEvents());
    QTRY_COMPARE(modelReady.count(), 1);
    QCOMPARE(model.rowCount(), 2);

    Event e;
    e = model.event(model.index(0, 0));
    QCOMPARE(e.recipients().count(), 1);
    QCOMPARE(e.recipients().at(0), Recipient::fromPhoneNumber(phone2));
    QCOMPARE(e.recipients().at(0).isContactResolved(), true);
    QCOMPARE(e.recipients().at(0).contactId(), 0);

    e = model.event(model.index(1, 0));
    QCOMPARE(e.recipients().count(), 1);
    QCOMPARE(e.recipients().at(0), Recipient::fromPhoneNumber(phone1));
    QCOMPARE(e.recipients().at(0).isContactResolved(), true);
    QCOMPARE(e.recipients().at(0).contactId(), 0);
}

void CallModelTest::testContactGrouping()
{
    deleteAll(false);

    ContactChangeListener contactChangeListener;

    const QString phone1("11111111");
    const QString phone2("22222222");
    const QString phone3("33333333");
    const QString phone4("44444444");
    const QString phone5("55555555");

    const QString user1("User1");
    const QString user2("User2");

    int user1Id = addTestContact(user1, phone1, RING_ACCOUNT, &contactChangeListener);
    int user2Id = addTestContact(user2, phone2, RING_ACCOUNT, &contactChangeListener);

    QVERIFY(addTestContactAddress(user1Id, phone4, RING_ACCOUNT));
    QVERIFY(addTestContactAddress(user2Id, phone5, RING_ACCOUNT));

    CallModel model;
    model.setFilter(CallModel::SortByContact);
    model.setResolveContacts(EventModel::ResolveOnDemand);

    QVERIFY(model.getEvents());
    QCOMPARE(model.rowCount(), 0);

    QDateTime when = QDateTime::currentDateTime();
    addTestEvent(model, Event::CallEvent, Event::Inbound, RING_ACCOUNT, -1, "", false, false, when, phone1);
    addTestEvent(model, Event::CallEvent, Event::Inbound, RING_ACCOUNT, -1, "", false, false, when.addSecs(1), phone2);
    addTestEvent(model, Event::CallEvent, Event::Inbound, RING_ACCOUNT, -1, "", false, false, when.addSecs(2), phone3);
    addTestEvent(model, Event::CallEvent, Event::Inbound, RING_ACCOUNT, -1, "", false, false, when.addSecs(3), phone4);
    addTestEvent(model, Event::CallEvent, Event::Inbound, RING_ACCOUNT, -1, "", false, false, when.addSecs(4), phone5);
    QTRY_COMPARE(model.rowCount(), 5);

    // Access the contactIds property of each event, which will force resolution
    (void)model.data(model.index(1, 0), CallModel::ContactIdsRole);
    (void)model.data(model.index(2, 0), CallModel::ContactIdsRole);
    (void)model.data(model.index(3, 0), CallModel::ContactIdsRole);

    QCOMPARE(model.rowCount(), 5);

    // Resolving this address will cause the event to be coalesced with a predecessor
    (void)model.data(model.index(4, 0), CallModel::ContactIdsRole);
    QTRY_COMPARE(model.rowCount(), 4);

    // Resolving this address will cause the event to be coalesced with a successor
    (void)model.data(model.index(0, 0), CallModel::ContactIdsRole);
    QTRY_COMPARE(model.rowCount(), 3);

    // Verify contact grouping after the UIDs are already resolved
    CallModel postModel;
    postModel.setFilter(CallModel::SortByContact);
    postModel.setResolveContacts(EventModel::ResolveOnDemand);

    QVERIFY(postModel.getEvents());
    QCOMPARE(postModel.rowCount(), 3);
}

void CallModelTest::cleanupTestCase()
{
    deleteAll();
}

QTEST_MAIN(CallModelTest)
