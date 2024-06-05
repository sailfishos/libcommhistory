#include <QtTest/QtTest>

#include <time.h>
#include "singleeventmodeltest.h"
#include "singleeventmodel.h"
#include "adaptor.h"
#include "event.h"
#include "common.h"

#include "modelwatcher.h"

using namespace CommHistory;

Group group1, group2;
QEventLoop loop;

ModelWatcher watcher;

void SingleEventModelTest::initTestCase()
{
    initTestDatabase();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    srand( QDateTime::currentDateTime().toSecsSinceEpoch() );
#else
    qsrand( QDateTime::currentDateTime().toTime_t() );
#endif

    addTestGroups(group1, group2);
}

void SingleEventModelTest::getEventById()
{
    SingleEventModel model;

    QSignalSpy modelReady(&model, &SingleEventModel::modelReady);
    watcher.setModel(&model);

    Event event;
    event.setType(Event::SMSEvent);
    event.setDirection(Event::Outbound);
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setGroupId(group1.id());
    event.setFreeText("freeText");
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setRecipients(Recipient(event.localUid(), "123456"));
    event.setMessageToken("messageTokenA1");

    // ignore call properties
    Event::PropertySet p = Event::allProperties();
    p.remove(Event::IsEmergencyCall);
    p.remove(Event::IsMissedCall);
    model.setPropertyMask(p);

    //TODO: add reading invalid id
    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());

    QVERIFY(event.id() != -1);
    QVERIFY(model.getEventById(event.id()));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();

    QCOMPARE(model.rowCount(), 1);

    Event modelEvent = model.event();
    QVERIFY(compareEvents(event, modelEvent));
}

void SingleEventModelTest::getEventByTokens()
{
    SingleEventModel model;
    // ignore call properties
    Event::PropertySet p = Event::allProperties();
    p.remove(Event::IsEmergencyCall);
    p.remove(Event::IsMissedCall);
    model.setPropertyMask(p);

    QSignalSpy modelReady(&model, &SingleEventModel::modelReady);
    watcher.setModel(&model);

    Event event;
    event.setType(Event::SMSEvent);
    event.setDirection(Event::Outbound);
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setGroupId(group1.id());
    event.setFreeText("freeText");
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setRecipients(Recipient(event.localUid(), "123456"));
    event.setMessageToken("messageTokenB1");

    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event.id() != -1);

    Event mms(event);
    mms.setMessageToken("mmsMessageToken");
    mms.setMmsId("mmsId");

    QVERIFY(model.addEvent(mms));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(mms.id() != -1);

    Event mms2(event);
    mms2.setMessageToken("mmsMessageToken");
    mms2.setMmsId("mmsId");
    mms2.setGroupId(group2.id());

    QVERIFY(model.addEvent(mms2));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(mms2.id() != -1);

    QVERIFY(model.getEventByTokens("messageTokenB1", "", -1));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();

    QCOMPARE(model.rowCount(), 1);

    Event modelEvent = model.event();
    QVERIFY(compareEvents(event, modelEvent));

    QVERIFY(model.getEventByTokens("messageTokenB1", "", group1.id()));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();

    QCOMPARE(model.rowCount(), 1);

    modelEvent = model.event();
    QVERIFY(compareEvents(event, modelEvent));

    QVERIFY(model.getEventByTokens("messageTokenB1", "", group1.id() + 1));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();

    QCOMPARE(model.rowCount(), 0);

    // Can match either to token or mms id:
    QVERIFY(model.getEventByTokens("messageTokenB1", "nonExistingMmsId", group1.id()));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();

    QCOMPARE(model.rowCount(), 1);

    modelEvent = model.event();
    QVERIFY(compareEvents(event, modelEvent));

    QVERIFY(model.getEventByTokens("", "nonExistingMmsId", group1.id()));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();

    QCOMPARE(model.rowCount(), 0);

    QVERIFY(model.getEventByTokens("", "mmsId", group1.id()));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();

    QCOMPARE(model.rowCount(), 1);

    modelEvent = model.event();
    QVERIFY(compareEvents(mms, modelEvent));

    QVERIFY(model.getEventByTokens("", "mmsId", group2.id()));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();

    QCOMPARE(model.rowCount(), 1);

    modelEvent = model.event();
    QVERIFY(compareEvents(mms2, modelEvent));

    QVERIFY(model.getEventByTokens("mmsMessageToken", "mmsId", group1.id()));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();

    QCOMPARE(model.rowCount(), 1);

    modelEvent = model.event();
    QVERIFY(compareEvents(mms, modelEvent));
}

void SingleEventModelTest::contactMatching_data()
{
    QTest::addColumn<QString>("localId");
    QTest::addColumn<QString>("remoteId");
    QTest::addColumn<int>("eventType");

    QTest::newRow("im") << "/org/freedesktop/Telepathy/Account/gabble/jabber/good_40localhost0"
            << "bad@singlelocalhost"
            << (int)Event::IMEvent;
    QTest::newRow("cell") << RING_ACCOUNT
            << "+11382394"
            << (int)Event::SMSEvent;

}

void SingleEventModelTest::contactMatching()
{
    QFETCH(QString, localId);
    QFETCH(QString, remoteId);
    QFETCH(int, eventType);

    ContactChangeListener contactChangeListener;

    SingleEventModel model;
    QSignalSpy modelReady(&model, &SingleEventModel::modelReady);
    model.setResolveContacts(EventModel::ResolveImmediately);

    Event::PropertySet p;
    p.insert(Event::ContactId);
    p.insert(Event::ContactName);
    model.setPropertyMask(p);

    watcher.setModel(&model);

    int eventId = addTestEvent(model, (Event::EventType)eventType, Event::Inbound, localId, group1.id(),
                 "text", false, false, QDateTime::currentDateTime(), remoteId);
    QVERIFY(watcher.waitForAdded());
    QVERIFY(eventId != -1);

    QVERIFY(model.getEventById(eventId));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();
    Event event = model.event();
    QCOMPARE(event.id(), eventId);
    QCOMPARE(event.contactId(), 0);

    // Add a non-matching contact and check the contact does not resolve in the model.
    int contactId1 = addTestContact("Really1Bad", remoteId + "123", localId, &contactChangeListener);
    QVERIFY(model.getEventById(eventId));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();
    event = model.event();
    QCOMPARE(event.id(), eventId);
    QCOMPARE(event.contactId(), 0);

    // Add a matching contact and check the contact is resolved in the model.
    int contactId = addTestContact("Really Bad", remoteId, localId, &contactChangeListener);

    QVERIFY(model.getEventById(eventId));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();
    QCOMPARE(event.id(), eventId);
    QCOMPARE(event.contactId(), contactId);
    QCOMPARE(event.contactName(), QString("Really Bad"));

    deleteTestContact(contactId1, &contactChangeListener);
    deleteTestContact(contactId, &contactChangeListener);
}

void SingleEventModelTest::updateStatus()
{
    SingleEventModel model;
    QSignalSpy modelReady(&model, &SingleEventModel::modelReady);
    watcher.setModel(&model);

    SingleEventModel observer;
    QSignalSpy observerModelReady(&model, &SingleEventModel::modelReady);

    Event event;
    event.setType(Event::SMSEvent);
    event.setDirection(Event::Outbound);
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setGroupId(group1.id());
    event.setFreeText("freeText");
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setRecipients(Recipient(event.localUid(), "123456"));
    event.setMessageToken("messageTokenB");

    QVERIFY(model.addEvent(event));
    QVERIFY(watcher.waitForAdded());
    QVERIFY(event.id() != -1);

    CommHistory::Event::PropertySet props = CommHistory::Event::PropertySet()
                                                      << CommHistory::Event::Id
                                                      << CommHistory::Event::Direction
                                                      << CommHistory::Event::Status
                                                      << CommHistory::Event::GroupId
                                                      << CommHistory::Event::MessageToken
                                                      << CommHistory::Event::ReportDelivery
                                                      << CommHistory::Event::MmsId;
    model.setPropertyMask(props);
    QVERIFY(model.getEventById(event.id()));
    QTRY_COMPARE(modelReady.count(), 1); modelReady.clear();

    QCOMPARE(model.rowCount(), 1);

    Event modelEvent = model.event();

    QVERIFY(modelEvent.validProperties().contains(CommHistory::Event::Status));
    QVERIFY(modelEvent.validProperties().contains(CommHistory::Event::MessageToken));

    QCOMPARE(event.status(), modelEvent.status());
    QCOMPARE(event.messageToken(), modelEvent.messageToken());


    // init observer model with the same event and all properties
    QVERIFY(observer.getEventById(event.id()));
    QTRY_COMPARE(observerModelReady.count(), 1);

    QCOMPARE(observer.rowCount(), 1);

    Event observedEvent = observer.event();

    QVERIFY(compareEvents(event, observedEvent));

    // modify event with new status only
    modelEvent.setStatus(Event::SentStatus);
    QVERIFY(model.modifyEvent(modelEvent));
    QVERIFY(watcher.waitForUpdated());

    //check observer model
    QTRY_COMPARE(observer.event().freeText(), event.freeText());
    QTRY_COMPARE(observer.event().status(), Event::SentStatus);
}

void SingleEventModelTest::cleanupTestCase()
{
    deleteAll();
}

QTEST_MAIN(SingleEventModelTest)
