/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2014-2015 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@jolla.com>
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

#include <time.h>
#include "eventmodeltest.h"
#include "eventmodel.h"
#include "conversationmodel.h"
#include "singleeventmodel.h"
#include "groupmodel.h"
#include "event.h"
#include "common.h"
#include "databaseio.h"

#include "modelwatcher.h"

Group group1, group2;
Event im, sms, call;

int groupUpdated = 0;
int groupDeleted = 0;

ModelWatcher watcher;

void EventModelTest::groupsUpdatedSlot(const QList<int> &groupIds)
{
    if (!groupIds.isEmpty())
        groupUpdated = groupIds.first();
}

void EventModelTest::groupsDeletedSlot(const QList<int> &groupIds)
{
    if (!groupIds.isEmpty())
        groupDeleted = groupIds.first();
}

void EventModelTest::initTestCase()
{
    initTestDatabase();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    srand(QDateTime::currentDateTime().toSecsSinceEpoch());
#else
    qsrand(QDateTime::currentDateTime().toTime_t());

    connect(&watcher, &UpdatesListener::groupsUpdated,
            this, &EventModelTest::groupsUpdatedSlot);
    connect(&watcher, &UpdatesListener::groupsDeleted,
            this, &EventModelTest::groupsDeletedSlot);

    addTestGroups(group1, group2);
}

void EventModelTest::testMessageToken()
{
    EventModel model;
    watcher.setModel(&model);

    sms.setGroupId(group1.id());
    sms.setType(Event::SMSEvent);
    sms.setDirection(Event::Outbound);
    sms.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    sms.setEndTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    sms.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    sms.setRecipients(Recipient(sms.localUid(), "123456"));
    sms.setFreeText("smstest");
    sms.setMessageToken("1234567890");
    QVERIFY(model.addEvent(sms));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(sms.id() != -1);
    Event event;
    QVERIFY(model.databaseIO().getEventByMessageToken(sms.messageToken(), event));
    QVERIFY(compareEvents(event, sms));
}

void EventModelTest::testAddEvent()
{
    EventModel model;
    watcher.setModel(&model);

    /* add invalid event */
    QVERIFY(!model.addEvent(im));
    im.setType(Event::IMEvent);
    /* missing direction, group id */
    QVERIFY(!model.addEvent(im));
    im.setDirection(Event::Outbound);
    /* missing group id */
    QVERIFY(!model.addEvent(im));

    /* add valid IM, SMS and call */
    im.setGroupId(group1.id());
    im.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    im.setEndTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    im.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    im.setRecipients(Recipient(im.localUid(), "td@localhost"));
    im.setFreeText("imtest");
    QVERIFY(model.addEvent(im));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(im.id() != -1);
    Event event;
    QVERIFY(model.databaseIO().getEvent(im.id(), event));
    QVERIFY(compareEvents(event, im));

    im.setIsAction(true);
    im.setId(-1);
    QVERIFY(model.addEvent(im));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(model.databaseIO().getEvent(im.id(), event));
    QVERIFY(compareEvents(event, im));
    QCOMPARE(event.isAction(), im.isAction());

    sms.setGroupId(group1.id());
    sms.setType(Event::SMSEvent);
    sms.setDirection(Event::Outbound);
    sms.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    sms.setEndTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    sms.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    sms.setRecipients(Recipient(sms.localUid(), "123456"));
    sms.setFreeText("smstest awefawef\nawefawefaw fawefawef \tawefawefawef awefawefawef awefawef");
    QVERIFY(model.addEvent(sms));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(sms.id() != -1);

    QVERIFY(model.databaseIO().getEvent(sms.id(), event));
    QVERIFY(compareEvents(event, sms));

    call.setType(Event::CallEvent);
    call.setDirection(Event::Outbound);
    call.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    call.setEndTime(QDateTime::fromString("2009-08-26T09:42:47Z", Qt::ISODate));
    call.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    call.setRecipients(Recipient(call.localUid(), "td@localhost"));
    call.setIsRead(true);
    QVERIFY(model.addEvent(call));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(call.id() != -1);

    QVERIFY(model.databaseIO().getEvent(call.id(), event));
    QVERIFY(compareEvents(event, call));

    // test setting non-existent group id
#if 0 // it's broken now because insert request is split in several and only one of them
      // has check for existent group
    sms.setGroupId(group1.id() + 9999999);
    QVERIFY(model.addEvent(sms));
    watcher.waitForSignals();
    QVERIFY(!model.trackerIO().getEvent(sms.id(), event));
#endif
}

void EventModelTest::testAddEvents()
{
    EventModel model;
    watcher.setModel(&model);

    QList<Event> events;
    Event e1, e2, e3;
    e1.setGroupId(group1.id());
    e1.setType(Event::IMEvent);
    e1.setDirection(Event::Outbound);
    e1.setStartTime(QDateTime::fromString("2010-01-08T13:37:00Z", Qt::ISODate));
    e1.setEndTime(QDateTime::fromString("2010-01-08T13:37:00Z", Qt::ISODate));
    e1.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    e1.setRecipients(Recipient(e1.localUid(), "td@localhost"));
    e1.setFreeText("addEvents 1");
    QHash<QString, QString> headers1;
    headers1.insert("header1_1", "value1_1");
    headers1.insert("header1_2", "value1_2");
    headers1.insert("x-mms-to", "foo@bar");
    e1.setHeaders(headers1);

    e2.setGroupId(group1.id());
    e2.setType(Event::IMEvent);
    e2.setDirection(Event::Inbound);
    e2.setStartTime(QDateTime::fromString("2010-01-08T13:37:10Z", Qt::ISODate));
    e2.setEndTime(QDateTime::fromString("2010-01-08T13:37:10Z", Qt::ISODate));
    e2.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    e2.setRecipients(Recipient(e2.localUid(), "td@localhost"));
    e2.setFreeText("addEvents 2");
    QHash<QString, QString> headers2;
    headers2.insert("header2_1", "value2_1");
    headers2.insert("header2_2", "value2_2");
    headers2.insert("x-mms-to", "mms@internet");
    e2.setHeaders(headers2);

    events << e1 << e2;
    QVERIFY(model.addEvents(events));
    QVERIFY(watcher.waitForAdded(events.size()));

    Event event;
    QVERIFY(model.databaseIO().getEvent(events[0].id(), event));
    QVERIFY(compareEvents(event, events[0]));
    QVERIFY(model.databaseIO().getEvent(events[1].id(), event));
    QVERIFY(compareEvents(event, events[1]));

    e3.setGroupId(group1.id());
    e3.setType(Event::IMEvent);
    e3.setDirection(Event::Inbound);
    e3.setStartTime(QDateTime::fromString("2010-01-08T13:37:20Z", Qt::ISODate));
    e3.setEndTime(QDateTime::fromString("2010-01-08T13:37:20Z", Qt::ISODate));
    e3.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    e3.setRecipients(Recipient(e3.localUid(), "td@localhost"));
    e3.setFreeText("addEvents 3");

    events.clear();
    events << e3;
    QVERIFY(model.addEvents(events,true)); // Add to model only, not into tracker.
    QVERIFY(watcher.waitForAdded(1, 0)); // 0 -> Do not wait for committed signal because we do not store
}

void EventModelTest::testModifyEvent()
{
    EventModel model;
    watcher.setModel(&model);

    im.setType(Event::IMEvent);
    im.setDirection(Event::Outbound);
    im.setGroupId(group1.id());
    im.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    im.setEndTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    im.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    im.setRecipients(Recipient(im.localUid(), "td@localhost"));
    im.setFreeText("imtest");
    QVERIFY(model.addEvent(im));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(im.id() != -1);

    Event event;
    QVERIFY(!model.modifyEvent(event));

    im.resetModifiedProperties();
    im.setFreeText("imtest \"q\" modified\t tabs");
    im.setStartTime(QDateTime::currentDateTime());
    im.setEndTime(QDateTime::currentDateTime());
    im.setIsRead(false);
    // should we actually test more properties?
    QVERIFY(model.modifyEvent(im));
    QVERIFY(watcher.waitForUpdated());

    QVERIFY(model.databaseIO().getEvent(im.id(), event));
    QVERIFY(compareEvents(im, event));

    // test mark read case
    im.resetModifiedProperties();
    im.setIsRead(true);
    QVERIFY(model.modifyEvent(im));
    QVERIFY(watcher.waitForUpdated());

    QVERIFY(model.databaseIO().getEvent(im.id(), event));
    QVERIFY(compareEvents(im, event));

    // test derlivery report case
    im.resetModifiedProperties();
    im.setDirection(Event::Outbound);
    im.setStatus(Event::SentStatus);
    im.setStartTime(QDateTime::currentDateTime());

    QVERIFY(model.modifyEvent(im));
    QVERIFY(watcher.waitForUpdated());

    QVERIFY(model.databaseIO().getEvent(im.id(), event));
    QVERIFY(compareEvents(im, event));

    im.setStatus(CommHistory::Event::DeliveredStatus);
    im.setEndTime(QDateTime::currentDateTime());

    QVERIFY(model.modifyEvent(im));
    QVERIFY(watcher.waitForUpdated());

    QVERIFY(model.databaseIO().getEvent(im.id(), event));
    QVERIFY(compareEvents(im, event));

    // headers
    QHash<QString, QString> headers;
    headers.insert("header1", "value1");
    headers.insert("header2", "value2");
    headers.insert("x-mms-to", "foo@bar");
    im.setHeaders(headers);
    QVERIFY(model.modifyEvent(im));
    QVERIFY(watcher.waitForUpdated());
    QVERIFY(model.databaseIO().getEvent(im.id(), event));
    QVERIFY(compareEvents(event, im));

    im.resetModifiedProperties();
    im.setToList(QStringList() << "to1" << "to2");
    QVERIFY(model.modifyEvent(im));
    QVERIFY(watcher.waitForUpdated());
    QVERIFY(model.databaseIO().getEvent(im.id(), event));
    QVERIFY(compareEvents(event, im));

    // call properties
    call.resetModifiedProperties();
    call.setIsMissedCall(true);
    call.setIsEmergencyCall(true);
    call.setIsVideoCall(true);

    QVERIFY(model.modifyEvent(call));
    QVERIFY(watcher.waitForUpdated());
    QVERIFY(model.databaseIO().getEvent(call.id(), event));
    QVERIFY(compareEvents(call, event));
}

void EventModelTest::testDeleteEvent()
{
    EventModel model;
    watcher.setModel(&model);

    Event event;
    QVERIFY(!model.deleteEvent(event));

    event.setType(Event::IMEvent);
    event.setDirection(Event::Inbound);
    event.setGroupId(group1.id());
    event.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    event.setEndTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setRecipients(Recipient(event.localUid(), "td@localhost"));
    event.setFreeText("deletetest");
    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event.id() != -1);

    QVERIFY(model.deleteEvent(event));
    QVERIFY(watcher.waitForDeleted());
    QVERIFY(!model.databaseIO().getEvent(event.id(), event));

    event.setType(Event::SMSEvent);
    event.setFreeText("deletetest sms");
    event.setRecipients(Recipient(event.localUid(), "555123456"));
    event.setGroupId(group2.id()); //group1 is gone, cause last event in it gone
    event.setDirection(Event::Inbound);
    event.setId(-1);
    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event.id() != -1);

    QVERIFY(model.deleteEvent(event));
    QVERIFY(watcher.waitForDeleted());
    QVERIFY(!model.databaseIO().getEvent(event.id(), event));
}

void EventModelTest::testDeleteEventVCard_data()
{
    QTest::addColumn<bool>("deleteGroups");

    QTest::newRow("Delete events") << false;
    QTest::newRow("Delete groups") << true;
}

void EventModelTest::testDeleteEventVCard()
{
    const QString VCARD_FILE_1("VCARD_FILE_1");
    const QString VCARD_LABEL_1("VCARD_LABEL_1");
    const QString VCARD_FILE_2("VCARD_FILE_2");
    const QString VCARD_LABEL_2("VCARD_LABEL_2");

    QFETCH(bool, deleteGroups);

    EventModel model;
    watcher.setModel(&model);

    int groupId1 = group1.id();
    int groupId2 = groupId1;

    if (deleteGroups) {
        Group g1;
        addTestGroup(g1,RING_ACCOUNT,QString("31415"));
        Group g2;
        addTestGroup(g2,RING_ACCOUNT,QString("18919"));
        groupId1 = g1.id();
        groupId2 = g2.id();
    }

    // test vcard resource deletion
    Event event;
    event.setType(Event::SMSEvent);
    event.setDirection(Event::Inbound);
    event.setGroupId(groupId1);
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setLocalUid(RING_ACCOUNT);
    event.setRecipients(Recipient::fromPhoneNumber("555123456"));
    event.setFreeText("vcard test");
    event.setFromVCard(VCARD_FILE_1, VCARD_LABEL_1);

    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event.id() != -1);

    int event1Id = event.id();

    event.setFromVCard(VCARD_FILE_2, VCARD_LABEL_2);
    event.setId(-1);
    event.setGroupId(groupId2);

    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event.id() != -1);

    int event2Id = event.id();

#if 0
    QScopedPointer<QSparqlConnection> conn(new QSparqlConnection(QLatin1String("QTRACKER_DIRECT")));
    QSparqlQuery fileNameQuery(QLatin1String("SELECT ?f {?f a nfo:FileDataObject; nfo:fileName ?:fileName}"));
    fileNameQuery.bindValue("fileName", VCARD_FILE_1);

    #define RUN_QUERY(query, resultSize) {\
    QSparqlResult* result = conn->exec(query);\
    result->waitForFinished();\
    QVERIFY(!result->hasError());\
    QCOMPARE(result->size(), resultSize);}

    RUN_QUERY(fileNameQuery, 1);

    fileNameQuery.bindValue("fileName", VCARD_FILE_2);

    RUN_QUERY(fileNameQuery, 1);
#endif

    GroupModel groupModel;
    QSignalSpy groupsCommitted(&groupModel, SIGNAL(groupsCommitted(QList<int>,bool)));

    if (deleteGroups) {
        groupModel.deleteGroups(QList<int>() << groupId1);
        QTRY_COMPARE(groupsCommitted.count(), 1);
        QVERIFY(groupsCommitted.first().at(1).toBool());
    } else {
        QVERIFY(model.deleteEvent(event1Id));
        QVERIFY(watcher.waitForDeleted());
    }

#if 0
    fileNameQuery.bindValue("fileName", VCARD_FILE_1);
    RUN_QUERY(fileNameQuery, 0);

    fileNameQuery.bindValue("fileName", VCARD_FILE_2);
    RUN_QUERY(fileNameQuery, 1);
#endif

    if (deleteGroups) {
        groupsCommitted.clear();
        groupModel.deleteGroups(QList<int>() << groupId2);
        QTRY_COMPARE(groupsCommitted.count(), 1);
        QVERIFY(groupsCommitted.first().at(1).toBool());
        groupsCommitted.clear();
    } else {
        QVERIFY(model.deleteEvent(event2Id));
        QVERIFY(watcher.waitForDeleted());
    }

#if 0
    fileNameQuery.bindValue("fileName", VCARD_FILE_2);
    RUN_QUERY(fileNameQuery, 0);

    #undef RUN_QUERY
#endif
}

void EventModelTest::testDeleteEventMmsParts_data()
{
    QTest::addColumn<bool>("deleteGroups");

    QTest::newRow("Delete events") << false;
    QTest::newRow("Delete groups") << true;
}

void EventModelTest::testDeleteEventMmsParts()
{
    QFETCH(bool, deleteGroups);

    EventModel model;
    watcher.setModel(&model);

    int groupId1 = group1.id();
    int groupId2 = groupId1;

    if (deleteGroups) {
        Group g1;
        addTestGroup(g1,RING_ACCOUNT,QString("15143"));
        Group g2;
        addTestGroup(g2,RING_ACCOUNT,QString("191828"));
        groupId1 = g1.id();
        groupId2 = g2.id();
    }

    // test vcard resource deletion
    Event event;
    event.setLocalUid(RING_ACCOUNT);
    event.setRecipients(Recipient::fromPhoneNumber("0506661234"));
    event.setType(Event::MMSEvent);
    event.setDirection(Event::Outbound);
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setFreeText("mms1");
    event.setGroupId(groupId1);
    event.setMessageToken("mms1token");

    MessagePart part1;
    part1.setContentId("blahSmil");
    part1.setContentType("application/smil");

    MessagePart part2;
    part2.setContentId("catphoto");
    part2.setContentType("image/jpeg");
    part2.setPath("/home/user/.mms/msgid001/catphoto.jpg");

    event.setMessageParts(QList<MessagePart>() << part1 << part2);

    QStringList toList1;
    toList1 << "111" << "222" << "to@mms.com";
    event.setToList(toList1);

    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event.id() != -1);

    int event1Id = event.id();

    MessagePart part3;
    part3.setContentId("text_slide2");
    part3.setContentType("text/plain");
    MessagePart part4;
    part4.setContentId("dogphoto");
    part4.setContentType("image/jpeg");
    part4.setPath("/home/user/.mms/msgid001/dogphoto.jpg");
    event.setId(-1);

    event.setMessageParts(QList<MessagePart>() << part3 << part4);

    QStringList toList2;
    toList2 << "333" << "444" << "to2@mms.com";
    event.setToList(toList2);
    event.setGroupId(groupId2);
    event.setMessageToken("mms2token");

    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event.id() != -1);

    int event2Id = event.id();

#if 0
    QScopedPointer<QSparqlConnection> conn(new QSparqlConnection(QLatin1String("QTRACKER_DIRECT")));

    #define RUN_QUERY(query, resultSize) {\
    QSparqlResult* result = conn->exec(query);\
    result->waitForFinished();\
    QVERIFY(!result->hasError());\
    QCOMPARE(result->size(), resultSize);}

    //check header
    QSparqlQuery headerQuery(QLatin1String("SELECT ?v {?h a nmo:MessageHeader; "
                                           "nmo:headerName \"x-mms-to\"; "
                                           "nmo:headerValue ?v "
                                           "FILTER(regex(?v, ?:pattern))}"));

    headerQuery.bindValue("pattern", toList1.first());
    RUN_QUERY(headerQuery, 1);

    headerQuery.bindValue("pattern", toList2.first());
    RUN_QUERY(headerQuery, 1);

    //check parts
    QSparqlQuery partsQuery(QLatin1String("SELECT ?a {?a a nmo:Attachment; "
                                           "nmo:contentId ?:contentId}"));
    #define CHECK_PART(part, result) {\
    partsQuery.bindValue("contentId", part.contentId()); \
    RUN_QUERY(partsQuery, result);}

    CHECK_PART(part1, 1);
    CHECK_PART(part2, 1);
    CHECK_PART(part3, 1);
    CHECK_PART(part4, 1);

    //check content
    QSparqlQuery contentQuery(QLatin1String("SELECT ?c {?c a nmo:Multipart; nie:hasPart [nmo:contentId ?:contentId]}"));
    #define CHECK_CONTENT(part, result) {\
    partsQuery.bindValue("contentId", part.contentId()); \
    RUN_QUERY(partsQuery, result);}

    CHECK_CONTENT(part1, 1);
    CHECK_CONTENT(part3, 1);
#endif

    GroupModel groupModel;
    QSignalSpy groupsCommitted(&groupModel, SIGNAL(groupsCommitted(QList<int>,bool)));

    if (deleteGroups) {
        groupModel.deleteGroups(QList<int>() << groupId1);
        QTRY_COMPARE(groupsCommitted.count(), 1);
        QVERIFY(groupsCommitted.first().at(1).toBool());
        groupsCommitted.clear();
    } else {
        QVERIFY(model.deleteEvent(event1Id));
        QVERIFY(watcher.waitForDeleted());
    }

#if 0
    headerQuery.bindValue("pattern", toList1.first());
    RUN_QUERY(headerQuery, 0);

    headerQuery.bindValue("pattern", toList2.first());
    RUN_QUERY(headerQuery, 1);

    CHECK_PART(part1, 0);
    CHECK_PART(part2, 0);
    CHECK_PART(part3, 1);
    CHECK_PART(part4, 1);

    CHECK_CONTENT(part1, 0);
    CHECK_CONTENT(part3, 1);
#endif

    if (deleteGroups) {
        groupsCommitted.clear();
        groupModel.deleteGroups(QList<int>() << groupId2);
        QTRY_COMPARE(groupsCommitted.count(), 1);
        QVERIFY(groupsCommitted.first().at(1).toBool());
        groupsCommitted.clear();
    } else {
        QVERIFY(model.deleteEvent(event2Id));
        QVERIFY(watcher.waitForDeleted());
    }

#if 0
    headerQuery.bindValue("pattern", toList2.first());
    RUN_QUERY(headerQuery, 0);

    CHECK_PART(part3, 0);
    CHECK_PART(part4, 0);

    CHECK_CONTENT(part3, 0);

    #undef RUN_QUERY
    #undef CHECK_PART
    #undef CHECK_CONTENT
#endif
}


void EventModelTest::testDeleteEventGroupUpdated()
{
    EventModel model;
    watcher.setModel(&model);

    GroupModel groupModel;
    groupModel.setResolveContacts(GroupManager::DoNotResolve);
    Group group;
    const QString LOCAL_ID("/org/freedesktop/Telepathy/Account/ring/tel/ring");
    const QString REMOTE_ID("12345");
    addTestGroup(group, LOCAL_ID, REMOTE_ID);

    QVERIFY(groupModel.databaseIO().getGroup(group.id(), group));

    groupUpdated = -1;
    groupDeleted = -1;

    Event event1;
    event1.setType(Event::SMSEvent);
    event1.setDirection(Event::Inbound);
    event1.setGroupId(group.id());
    event1.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    event1.setEndTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    event1.setLocalUid(LOCAL_ID);
    event1.setRecipients(Recipient(LOCAL_ID, REMOTE_ID));
    event1.setFreeText("deletetest1");
    QVERIFY(model.addEvent(event1));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event1.id() != -1);

    QVERIFY(groupModel.databaseIO().getGroup(group.id(), group));
    QCOMPARE(group.lastEventId(), event1.id());

    Event event2(event1);
    event2.setId(-1);
    event2.setFreeText("deletetest2");
    event2.setStartTime(event1.startTime().addSecs(10));
    event2.setEndTime(event1.endTime().addSecs(10));
    QVERIFY(model.addEvent(event2));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event2.id() != -1);

    QVERIFY(groupModel.databaseIO().getGroup(group.id(), group));
    QCOMPARE(group.lastEventId(), event2.id());

    QVERIFY(model.deleteEvent(event1.id()));
    QVERIFY(watcher.waitForDeleted());
    QTRY_COMPARE(groupUpdated, group.id());

    QVERIFY(groupModel.databaseIO().getGroup(group.id(), group));

    QVERIFY(model.deleteEvent(event2.id()));
    QVERIFY(watcher.waitForDeleted());
    QTRY_COMPARE(groupDeleted, group.id());

    QVERIFY(!groupModel.databaseIO().getGroup(group.id(), group));

    addTestGroup(group, LOCAL_ID, REMOTE_ID);

    QVERIFY(groupModel.databaseIO().getGroup(group.id(), group));

    event1.setId(-1);
    event1.setGroupId(group.id());
    QVERIFY(model.addEvent(event1));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event1.id() != -1);

    QVERIFY(groupModel.databaseIO().getGroup(group.id(), group));
    QCOMPARE(group.lastEventId(), event1.id());

    QVERIFY(model.deleteEvent(event1));
    QVERIFY(watcher.waitForDeleted());

    QVERIFY(!groupModel.databaseIO().getGroup(group.id(), group));
}

void EventModelTest::testVCard()
{
    QString vcardFilename1( "filename.vcd" );
    QString vcardFilename2( "filename.txt" );
    QString vcardLabel( "Test Label" );

    EventModel model;
    watcher.setModel(&model);

    Event event;
    QVERIFY( !model.deleteEvent( event ) );

    // create test data
    event.setType( Event::SMSEvent );
    event.setDirection( Event::Inbound );
    event.setGroupId( group1.id() );
    event.setStartTime( QDateTime::currentDateTime() );
    event.setEndTime( QDateTime::currentDateTime() );
    event.setLocalUid( "/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0" );
    event.setRecipients( Recipient( event.localUid(),  "555123456") );
    event.setFreeText( "vcard test" );
    event.setFromVCard( vcardFilename1, vcardLabel );

    // add event with vcard
    QVERIFY( model.addEvent( event ) );
    // test is event was added successfully
    QVERIFY( watcher.waitForAdded() );
    QVERIFY( event.id() != -1 );

    // fetch event to check vcard data
    {
        Event test_event;
        QVERIFY( !test_event.isValid() );
        QVERIFY( model.databaseIO().getEvent( event.id(), test_event ) );

        QVERIFY( test_event.isValid() );
        QVERIFY( compareEvents( event, test_event ) );
    }

    // modify data
    event.setFromVCard( vcardFilename2 );
    QVERIFY( model.modifyEvent( event ) );
    QVERIFY( watcher.waitForUpdated() );

    // fetch event to check vcard data
    {
        Event test_event;
        QVERIFY( !test_event.isValid() );
        QVERIFY( model.databaseIO().getEvent( event.id(), test_event ) );

        QVERIFY( test_event.isValid() );
        QVERIFY( compareEvents( event, test_event ) );
    }

    QVERIFY( model.deleteEvent( event ) );
    QVERIFY( watcher.waitForDeleted() );
    QVERIFY(!model.databaseIO().getEvent(event.id(), event));
}

void EventModelTest::testDeliveryStatus()
{
    EventModel model;
    watcher.setModel(&model);

    Event event;
    event.setType(Event::SMSEvent);
    event.setDirection(Event::Outbound);
    event.setGroupId(group1.id());
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setRecipients(Recipient(event.localUid(), "55590210"));
    event.setFreeText("delivery status test");
    event.setStatus(Event::SendingStatus);
    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event.id() != -1);

    Event e;
    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(e.status() == Event::SendingStatus);

    event.setStatus(Event::SentStatus);
    QVERIFY(model.modifyEvent(event));
    QVERIFY(watcher.waitForUpdated());
    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(e.status() == Event::SentStatus);

    event.setStatus(Event::DeliveredStatus);
    QVERIFY(model.modifyEvent(event));
    QVERIFY(watcher.waitForUpdated());
    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(e.status() == Event::DeliveredStatus);

    event.setStatus(Event::TemporarilyFailedStatus);
    QVERIFY(model.modifyEvent(event));
    QVERIFY(watcher.waitForUpdated());
    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(e.status() == Event::TemporarilyFailedStatus);

    event.setStatus(Event::TemporarilyFailedOfflineStatus);
    QVERIFY(model.modifyEvent(event));
    QVERIFY(watcher.waitForUpdated());
    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(e.status() == Event::TemporarilyFailedOfflineStatus);

    event.setStatus(Event::PermanentlyFailedStatus);
    QVERIFY(model.modifyEvent(event));
    QVERIFY(watcher.waitForUpdated());
    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(e.status() == Event::PermanentlyFailedStatus);

    event.setStatus(Event::SendingStatus);
    QVERIFY(model.modifyEvent(event));
    QVERIFY(watcher.waitForUpdated());
    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(e.status() == Event::SendingStatus);
}

void EventModelTest::testReportDelivery()
{
    EventModel model;
    watcher.setModel(&model);

    Event event;
    event.setType(Event::SMSEvent);
    event.setDirection(Event::Outbound);
    event.setGroupId(group1.id());
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setRecipients(Recipient(event.localUid(), "555888999"));
    event.setFreeText("report delivery test");
    event.setReportDelivery(true);

    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event.id() != -1);

    Event e;
    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(e.reportDelivery());

    event.setReportDelivery(false);

    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event.id() != -1);

    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(!e.reportDelivery());
}

void EventModelTest::testMessageParts()
{
    EventModel model;
    watcher.setModel(&model);

    Event event;
    event.setLocalUid("/org/freedesktop/Telepathy/Account/ring/tel/ring");
    event.setRecipients(Recipient(event.localUid(), "0506661234"));
    event.setType(Event::MMSEvent);
    event.setDirection(Event::Outbound);
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setFreeText("mms");
    event.setGroupId(group1.id());

    QList<MessagePart> parts;

    MessagePart part1;
    part1.setContentId("catphoto");
    part1.setContentType("image/jpeg");
    part1.setPath("/home/user/.mms/msgid001/catphoto.jpg");
    MessagePart part2;
    part2.setContentId("dogphoto");
    part2.setContentType("image/jpeg");
    part2.setPath("/home/user/.mms/msgid001/dogphoto.jpg");

    parts << part1 << part2;
    event.setMessageParts(parts);

    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event.id() != -1);

    parts = event.messageParts();
    QVERIFY(parts[0].id() >= 0);
    QVERIFY(parts[1].id() >= 0);

    Event e;
    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(compareEvents(event, e));
    QCOMPARE(e.messageParts().size(), parts.size());
    foreach (MessagePart part, e.messageParts())
        QVERIFY(parts.indexOf(part) >= 0);

    // modify message parts
    parts[0].setContentId("newcatphoto");
    parts[0].setPath("/home/user/.mms/msgid001/catphoto.jpg");
    event.setMessageParts(parts);
    QVERIFY(model.modifyEvent(event));
    QVERIFY(watcher.waitForUpdated());

    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(compareEvents(event, e));
    QCOMPARE(e.messageParts().size(), parts.size());
    foreach (MessagePart part, e.messageParts())
        QVERIFY(parts.indexOf(part) >= 0);

    // remove message part
    parts.takeFirst();
    event.setMessageParts(parts);

    QVERIFY(model.modifyEvent(event));
    QVERIFY(watcher.waitForUpdated());

    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(compareEvents(event, e));
    QCOMPARE(e.messageParts().size(), parts.size());
    foreach (MessagePart part, e.messageParts())
        QVERIFY(parts.indexOf(part) >= 0);
}

void EventModelTest::testCcBcc()
{
    EventModel model;
    watcher.setModel(&model);

    Event event;
    event.setLocalUid("/org/freedesktop/Telepathy/Account/ring/tel/ring");
    event.setRecipients(Recipient(event.localUid(), "0506661234"));
    event.setType(Event::MMSEvent);
    event.setDirection(Event::Outbound);
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setFreeText("mms");
    event.setGroupId(group1.id());

    QStringList ccList;
    ccList << "+12345" << "98765" << "cc@mms.com";
    event.setCcList(ccList);

    QStringList bccList;
    bccList << "+777888" << "999888" << "bcc@mms.com";
    event.setBccList(bccList);

    QStringList toList;
    toList << "+10111" << "+10112" << "to@mms.com";
    event.setToList(toList);

    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event.id() != -1);

    Event e;
    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(compareEvents(event, e));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QCOMPARE(QSet<QString>(e.ccList().begin(),e.ccList().end()) , QSet<QString>(ccList.begin(), ccList.end()));
    QCOMPARE(QSet<QString>(e.bccList().begin(),e.bccList().end()) , QSet<QString>(bccList.begin(), bccList.end()));
    QCOMPARE(QSet<QString>(e.toList().begin(),e.toList().end()) , QSet<QString>(toList.begin(), toList.end()));
#else
    QCOMPARE(e.ccList().toSet(), ccList.toSet());
    QCOMPARE(e.bccList().toSet(), bccList.toSet());
    QCOMPARE(e.toList().toSet(), toList.toSet());
#endif
    event.resetModifiedProperties();
    ccList.clear();
    ccList << "112" << "358" << "mcc@mms.com";
    event.setCcList(ccList);

    bccList.clear();
    bccList << "314" << "15" << "mbcc@mms.com";
    event.setBccList(bccList);

    toList.clear();
    toList << "777" << "888" << "mto@mms.com";
    event.setToList(toList);

    QVERIFY(model.modifyEvent(event));
    QVERIFY(watcher.waitForUpdated());
    QVERIFY(event.id() != -1);

    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(compareEvents(event, e));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QCOMPARE(QSet<QString>(e.ccList().begin(),e.ccList().end()) , QSet<QString>(ccList.begin(), ccList.end()));
    QCOMPARE(QSet<QString>(e.bccList().begin(),e.bccList().end()) , QSet<QString>(bccList.begin(), bccList.end()));
    QCOMPARE(QSet<QString>(e.toList().begin(),e.toList().end()) , QSet<QString>(toList.begin(), toList.end()));
#else
    QCOMPARE(e.ccList().toSet(), ccList.toSet());
    QCOMPARE(e.bccList().toSet(), bccList.toSet());
    QCOMPARE(e.toList().toSet(), toList.toSet());
#endif

    event.resetModifiedProperties();
    ccList.clear();
    event.setCcList(ccList);
    event.setBccList(ccList);
    event.setToList(ccList);

    QVERIFY(model.modifyEvent(event));
    QVERIFY(watcher.waitForUpdated());
    QVERIFY(event.id() != -1);

    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(e.ccList().isEmpty());
    QVERIFY(e.bccList().isEmpty());
    QVERIFY(e.toList().isEmpty());
}


void EventModelTest::testFindEvent()
{
    ConversationModel model;
    Event event;

    model.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model.getEvents(group1.id()));
    QModelIndex index = model.findEvent(im.id());
    QVERIFY(index.isValid());
    event = index.data(Qt::UserRole).value<Event>();
    QVERIFY(compareEvents(event, im));

    // Test also EventModel's data-method for majority and most important event fields:
    int row = index.row();
    QModelIndex index2 = model.index(row);
    QVariant var2 = model.data(index2, EventModel::EventIdRole);
    int eventId = var2.toInt();
    QCOMPARE(eventId, im.id());

    index2 = model.index(row);
    var2 = model.data(index2, EventModel::EventTypeRole);
    int eventType = var2.toInt();
    QCOMPARE(eventType, (int) im.type());

    index2 = model.index(row);
    var2 = model.data(index2, EventModel::DirectionRole);
    int direction = var2.toInt();
    QCOMPARE(direction, (int) im.direction());

    index2 = model.index(row);
    var2 = model.data(index2, EventModel::IsReadRole);
    bool isRead = var2.toBool();
    QCOMPARE(isRead, im.isRead());

    index2 = model.index(row);
    var2 = model.data(index2, EventModel::StatusRole);
    int status = var2.toInt();
    QCOMPARE(status, (int) im.status());

    index2 = model.index(row);
    var2 = model.data(index2, EventModel::LocalUidRole);
    QString localUid = var2.toString();
    QCOMPARE(localUid, im.localUid());

    index2 = model.index(row);
    var2 = model.data(index2, EventModel::RemoteUidRole);
    QString remoteUid = var2.toString();
    QCOMPARE(remoteUid, im.recipients().value(0).remoteUid());

    index2 = model.index(row);
    var2 = model.data(index2, EventModel::FreeTextRole);
    QString freeText = var2.toString();
    QCOMPARE(freeText, im.freeText());

    index2 = model.index(row);
    var2 = model.data(index2, EventModel::GroupIdRole);
    int groupId = var2.toInt();
    QCOMPARE(groupId, im.groupId());

    index2 = model.index(row);
    var2 = model.data(index2, EventModel::MessageTokenRole);
    QString messageToken = var2.toString();
    QCOMPARE(messageToken, im.messageToken());

    index2 = model.index(row);
    var2 = model.data(index2, EventModel::StartTimeRole);
    QDateTime startTime = var2.toDateTime();
    QCOMPARE(startTime.toMSecsSinceEpoch() / 1000, im.startTime().toMSecsSinceEpoch() / 1000);

    index = model.findEvent(-1);
    QVERIFY(!index.isValid());
}

void EventModelTest::testMoveEvent()
{
    addTestGroup(group2,
                 "/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0",
                 "td2@localhost");

    groupDeleted = -1;

    EventModel model;
    watcher.setModel(&model);
    Event event;
    event.setGroupId(group1.id());
    event.setType(Event::IMEvent);
    event.setDirection(Event::Inbound);
    event.setStartTime(QDateTime::fromString("2010-01-08T13:39:00Z", Qt::ISODate));
    event.setEndTime(QDateTime::fromString("2010-01-08T13:39:00Z", Qt::ISODate));
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setRecipients(Recipient(event.localUid(), "td@localhost"));
    event.setFreeText("moveEvent 1");

    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());

    QVERIFY(model.moveEvent(event,group2.id()));
    QVERIFY(watcher.waitForCommitted());
    QCOMPARE(event.groupId(), group2.id());

    Event eventFromTracker;
    QVERIFY(model.databaseIO().getEvent(event.id(), eventFromTracker));
    QCOMPARE(eventFromTracker.groupId(), group2.id());

    // Try with invalid event:
    Event invalidEvent;
    QVERIFY(!model.moveEvent(invalidEvent,group2.id()));

    // Try to move event to the same group where event already is:
    QVERIFY(model.moveEvent(eventFromTracker,group2.id()));

    // Moving event from a group that has this event as the only one.
    // Then the empty group should be deleted:
    QVERIFY(model.moveEvent(eventFromTracker,group1.id()));

    QVERIFY(watcher.waitForCommitted());
    QTRY_COMPARE(groupDeleted, group2.id());
}

void EventModelTest::testStreaming_data()
{
    QTest::addColumn<bool>("useThread");

    // FIXME: skip for now - can fail randomly (check convmodel-streaming-fix branch)
    QTest::newRow("Without thread") << false;
    //QTest::newRow("Use thread") << true;
}

void EventModelTest::testStreaming()
{
    QFETCH(bool, useThread);

    GroupModel groupModel;
    groupModel.setResolveContacts(GroupManager::DoNotResolve);
    Group group;

    QThread modelThread;

    QVERIFY(groupModel.databaseIO().getGroup(group1.id(), group));
    int total;
    QVERIFY(groupModel.databaseIO().totalEventsInGroup(group1.id(), total));

    ConversationModel model;
    model.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model.getEvents(group1.id()));

    ConversationModel streamModel;

    if (useThread) {
        modelThread.start();
        streamModel.setBackgroundThread(&modelThread);
    }

    streamModel.setQueryMode(EventModel::StreamedAsyncQuery);
    const int normalChunkSize = 4;
    const int firstChunkSize = 2;
    streamModel.setChunkSize(normalChunkSize);
    streamModel.setFirstChunkSize(firstChunkSize);
    qRegisterMetaType<QModelIndex>("QModelIndex");
    QSignalSpy rowsInserted(&streamModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)));
    QSignalSpy modelReady(&streamModel, SIGNAL(modelReady(bool)));
    QVERIFY(streamModel.getEvents(group1.id()));

    int count = 0;
    int chunkSize = firstChunkSize;
    while (count < total) {
        if (count > 0)
            chunkSize = normalChunkSize;

        int expectedEnd = count + chunkSize > total ? total - 1: count + chunkSize - 1;

        int lastInserted = -1;
        int firstInserted = -1;

        while (expectedEnd != lastInserted) {
            QTRY_VERIFY(rowsInserted.count() > 0);

            if (firstInserted == -1)
                firstInserted = rowsInserted.first().at(1).toInt();
            lastInserted = rowsInserted.last().at(2).toInt(); // end
            rowsInserted.clear();
        }

        QCOMPARE(firstInserted, count); // start

        for (int i = count; i < expectedEnd + 1; i++) {
            Event event1 = model.index(i, 0).data(Qt::UserRole).value<Event>();
            Event event2 = streamModel.index(i, 0).data(Qt::UserRole).value<Event>();
            QVERIFY(compareEvents(event1, event2));
        }

        // You should be able to fetch more if total number of messages is not yet reached:
        if ( lastInserted < total-1 )
        {
            QVERIFY(streamModel.canFetchMore(QModelIndex()));
            QVERIFY(modelReady.isEmpty());
            streamModel.fetchMore(QModelIndex());
        }

        count += chunkSize;
    }

    if (streamModel.canFetchMore(QModelIndex()))
        streamModel.fetchMore(QModelIndex());

    QTRY_COMPARE(modelReady.count(), 1);
    QVERIFY(!streamModel.canFetchMore(QModelIndex()));
    QCOMPARE(streamModel.rowCount(), total);

    modelThread.quit();
    modelThread.wait(3000);
}

void EventModelTest::testModifyInGroup()
{
    EventModel model;
    watcher.setModel(&model);

    Event event;
    event.setType(Event::SMSEvent);
    event.setGroupId(group1.id());
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setRecipients(Recipient(event.localUid(), "td@localhost"));
    event.setFreeText("imtest");
    event.setDirection(Event::Outbound);
    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());

    Group group;
    GroupModel groupModel;
    groupModel.setResolveContacts(GroupManager::DoNotResolve);

    QVERIFY(groupModel.databaseIO().getGroup(group1.id(), group));
    QCOMPARE(group.lastEventId(), event.id());
    QCOMPARE(group.lastMessageText(), event.freeText());
    QCOMPARE(group.lastEventType(), event.type());
    QCOMPARE(group.lastEventStatus(), Event::UnknownStatus);

    // change status
    event.setStatus(Event::SentStatus);
    QVERIFY(model.modifyEventsInGroup(QList<Event>() << event, group));
    QVERIFY(watcher.waitForUpdated());

    QVERIFY(groupModel.databaseIO().getGroup(group1.id(), group));
    QCOMPARE(group.lastEventId(), event.id());
    QCOMPARE(group.lastMessageText(), event.freeText());
    QCOMPARE(group.lastEventType(), event.type());
    QCOMPARE(group.lastEventStatus(), Event::SentStatus);

    Event e;
    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(compareEvents(event, e));


    // change text
    event.setFreeText("modified text");
    QVERIFY(model.modifyEventsInGroup(QList<Event>() << event, group));
    QVERIFY(watcher.waitForUpdated());

    QVERIFY(groupModel.databaseIO().getGroup(group1.id(), group));
    QCOMPARE(group.lastEventId(), event.id());
    QVERIFY(group.lastMessageText() == "modified text");
    QCOMPARE(group.lastEventType(), event.type());
    QCOMPARE(group.lastEventStatus(), Event::SentStatus);

    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(compareEvents(event, e));

    // change vcard
    event.setFromVCard("vcard.txt", "Oki Doki");

    QVERIFY(model.modifyEventsInGroup(QList<Event>() << event, group));
    QVERIFY(watcher.waitForUpdated());

    QVERIFY(groupModel.databaseIO().getGroup(group1.id(), group));
    QCOMPARE(group.lastEventId(), event.id());
    QVERIFY(group.lastVCardFileName() == "vcard.txt");
    QVERIFY(group.lastVCardLabel() == "Oki Doki");

    QVERIFY(model.databaseIO().getEvent(event.id(), e));
    QVERIFY(compareEvents(event, e));

    // test unread count updating
    int unread = group.unreadMessages();

    Event newEvent;
    newEvent.setGroupId(group1.id());
    newEvent.setType(Event::IMEvent);
    newEvent.setDirection(Event::Inbound);
    newEvent.setStartTime(QDateTime::currentDateTime().addSecs(5));
    newEvent.setEndTime(QDateTime::currentDateTime().addSecs(5));
    newEvent.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    newEvent.setRecipients(Recipient(event.localUid(), "td@localhost"));
    newEvent.setFreeText("addEvents 2");

    QVERIFY(model.addEvent(newEvent));
    QVERIFY(watcher.waitForAdded());

    QVERIFY(groupModel.databaseIO().getGroup(group1.id(), group));
    QCOMPARE(group.lastEventId(), newEvent.id());
    QCOMPARE(group.unreadMessages(), unread + 1);

    // mark as read
    newEvent.setIsRead(true);

    QVERIFY(model.modifyEventsInGroup(QList<Event>() << newEvent, group));
    QVERIFY(watcher.waitForUpdated());

    QVERIFY(groupModel.databaseIO().getGroup(group1.id(), group));
    QCOMPARE(group.lastEventId(), newEvent.id());
    QCOMPARE(group.unreadMessages(), unread);
}

void EventModelTest::testExtraProperties()
{
    EventModel model;
    watcher.setModel(&model);

    Event newEvent;
    newEvent.setGroupId(group1.id());
    newEvent.setType(Event::IMEvent);
    newEvent.setDirection(Event::Inbound);
    newEvent.setStartTime(QDateTime::currentDateTime().addSecs(6));
    newEvent.setEndTime(newEvent.startTime());
    newEvent.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    newEvent.setRecipients(Recipient(newEvent.localUid(), "td@localhost"));
    newEvent.setExtraProperty("testing", 42);

    QVERIFY(model.addEvent(newEvent));
    QVERIFY(watcher.waitForAdded());

    // Read property
    SingleEventModel model2;
    QVERIFY(model2.getEventById(newEvent.id()));
    Event returnedEvent = model2.event();
    QVERIFY(returnedEvent.isValid());
    QCOMPARE(returnedEvent.extraProperties().size(), 1);
    QCOMPARE(returnedEvent.extraProperty("testing").toInt(), 42);

    // Remove property
    newEvent.setExtraProperty("testing", QVariant());
    QVERIFY(newEvent.extraProperties().isEmpty());
    QVERIFY(newEvent.modifiedProperties().contains(Event::ExtraProperties));
    QVERIFY(model.modifyEvent(newEvent));
    QVERIFY(watcher.waitForUpdated());

    // Re-read property
    QVERIFY(model2.getEventById(newEvent.id()));
    returnedEvent = model2.event();
    QVERIFY(returnedEvent.isValid());
    QVERIFY(returnedEvent.extraProperties().isEmpty());
}

void EventModelTest::testContactMatching_data()
{
    QTest::addColumn<QString>("localId");
    QTest::addColumn<QString>("remoteId");
    QTest::addColumn<int>("eventType");

    QTest::newRow("im") << "/org/freedesktop/Telepathy/Account/gabble/jabber/good_40localhost0"
            << "bad@localhost"
            << (int)Event::IMEvent;
    QTest::newRow("cell") << RING_ACCOUNT
            << "+42382394"
            << (int)Event::SMSEvent;
}

void EventModelTest::testContactMatching()
{
    QFETCH(QString, localId);
    QFETCH(QString, remoteId);
    QFETCH(int, eventType);

    ContactChangeListener contactChangeListener;

    EventModel model;
    model.setResolveContacts(EventModel::ResolveImmediately);
    model.setDefaultAccept(true);

    watcher.setModel(&model);

    int eventId = addTestEvent(model, (Event::EventType)eventType, Event::Inbound, localId, group1.id(),
                 "text", false, false, QDateTime::currentDateTime(), remoteId);
    QVERIFY(watcher.waitForAdded());
    QVERIFY(eventId != -1);

    Event event;
    model.databaseIO().getEvent(eventId, event);
    QCOMPARE(event.contactId(), 0);

    // Add a non-matching contact and check the contact does not resolve in the model.
    int contactId1 = addTestContact("Really Bad", remoteId + "123", localId, &contactChangeListener);
    event = model.event(model.findEvent(eventId));
    QCOMPARE(event.contactId(), 0);
    QCOMPARE(event.contactName(), QString());

    int contactId = addTestContact("Really Bad", remoteId, localId, &contactChangeListener);
    event = model.event(model.findEvent(eventId));
    QCOMPARE(event.contactId(), contactId);
    QCOMPARE(event.contactName(), QString("Really Bad"));

    // If a new contact is added with the same address, it will replace the previous resolution
    int replacementContactId = addTestContact("Moderately Bad", remoteId, localId, &contactChangeListener);
    event = model.event(model.findEvent(eventId));
    QCOMPARE(event.contactId(), replacementContactId);
    QCOMPARE(event.contactName(), QString("Moderately Bad"));

    // After the contacts are removed, the events resolve to nothing
    deleteTestContact(contactId1);
    deleteTestContact(contactId);
    deleteTestContact(replacementContactId);

    QTRY_COMPARE(model.event(model.findEvent(eventId)).contactId(), 0);
    QTRY_COMPARE(model.event(model.findEvent(eventId)).contactName(), QString());
}

void EventModelTest::testAddNonDigitRemoteId_data()
{
    QTest::addColumn<QString>("localId");
    QTest::addColumn<QString>("remoteId");

    QTest::newRow("vm") << RING_ACCOUNT << "voicemail";
    QTest::newRow("space") << RING_ACCOUNT << "3 voicemail";
}

void EventModelTest::testAddNonDigitRemoteId()
{
    QFETCH(QString, localId);
    QFETCH(QString, remoteId);

    Group g;
    addTestGroup(g, localId, remoteId);

    EventModel model;
    Group tg;
    QVERIFY(model.databaseIO().getGroup(g.id(), tg));
    QCOMPARE(tg.id(), g.id());
    QCOMPARE(tg.localUid(), tg.localUid());
    QCOMPARE(tg.recipients(), tg.recipients());

    watcher.setModel(&model);
    Event event;
    event.setGroupId(g.id());
    event.setType(Event::SMSEvent);
    event.setDirection(Event::Inbound);
    event.setStartTime(QDateTime::fromString("2010-01-08T13:39:00Z", Qt::ISODate));
    event.setEndTime(QDateTime::fromString("2010-01-08T13:39:00Z", Qt::ISODate));
    event.setLocalUid(localId);
    event.setRecipients(Recipient(localId, remoteId));
    event.setFreeText("you have a new voicemail");

    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());

    Event tevent;
    QVERIFY(model.databaseIO().getEvent(event.id(), tevent));
    QVERIFY(compareEvents(event, tevent));
}

void EventModelTest::testBufferInsertions()
{
    EventModel model;
    model.setDefaultAccept(true);

    watcher.setModel(&model);

    QSignalSpy rowsInserted(&model, SIGNAL(rowsInserted(const QModelIndex &, int, int)));

    QCOMPARE(model.bufferInsertions(), false);

    /* Add a valid event */
    im.setType(Event::IMEvent);
    im.setDirection(Event::Outbound);
    im.setGroupId(group1.id());
    im.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    im.setEndTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    im.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    im.setRecipients(Recipient(im.localUid(), "td@localhost"));
    im.setFreeText("imtest");
    QVERIFY(model.addEvent(im));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(im.id() != -1);

    /* A row should have been inserted */
    QTRY_COMPARE(rowsInserted.count(), 1);
    QCOMPARE(rowsInserted.first().at(1).toInt(), 0);
    rowsInserted.clear();

    Event event;
    QVERIFY(model.databaseIO().getEvent(im.id(), event));
    QVERIFY(compareEvents(event, im));

    /* Set insertion buffering */
    model.setBufferInsertions(true);
    QCOMPARE(model.bufferInsertions(), true);

    /* Add a second event, which is buffered */
    im.setId(-1);
    QVERIFY(model.addEvent(im));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(im.id() != -1);

    /* No rows should have been added.
      Do not clear rowsInserted count before the next test. */

    /* Turn off buffering so the event is added to the model */
    model.setBufferInsertions(false);
    QCOMPARE(model.bufferInsertions(), false);

    /* A row should now be reported as inserted */
    QTRY_COMPARE(rowsInserted.count(), 1);
    QCOMPARE(rowsInserted.first().at(1).toInt(), 0);
    rowsInserted.clear();

    /* Add an unbuffered event */
    im.setId(-1);
    QVERIFY(model.addEvent(im));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(im.id() != -1);
    QTRY_COMPARE(rowsInserted.count(), 1);
    QCOMPARE(rowsInserted.first().at(1).toInt(), 0);
    rowsInserted.clear();
}

void EventModelTest::cleanupTestCase()
{
    deleteAll();

    if (call.id() != -1) {
        // TODO: clean-up is broken. Other test events are only cleaned up because
        // they are assigned a groupId (compare cleanupTestGroups() with
        // cleanupTestEvents from common.cpp).  'call' event is not set a groupId
        // and so it is not cleaned-up upon exit. Deleting 'call' event here as
        // a QUICK FIX!
        QVERIFY(EventModel().deleteEvent(call));
    }
}

QTEST_MAIN(EventModelTest)
