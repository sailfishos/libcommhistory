#include <QtTest/QtTest>

#include <time.h>
#include "recentcontactsmodeltest.h"
#include "recentcontactsmodel.h"
#include "adaptor.h"
#include "event.h"
#include "common.h"
#include "databaseio.h"

#include "modelwatcher.h"

using namespace CommHistory;

Group group1, group2;
ModelWatcher watcher;

int aliceId, bobId, charlieId;
const QString aliceName("Alice");
const QString alicePhone1("1234567");
const QString alicePhone2("2345678");
const QString bobName("Bob");
const QString bobPhone("9876543");
const QPair<QString, QString> bobIm(qMakePair(ACCOUNT1, QString::fromLatin1("bob@example.org")));
const QString charlieName("Charlie");
const QPair<QString, QString> charlieIm1(qMakePair(ACCOUNT1, QString::fromLatin1("charlie@example.org")));
const QPair<QString, QString> charlieIm2(qMakePair(ACCOUNT1, QString::fromLatin1("douglas@example.org")));
const QString phoneAccount(RING_ACCOUNT);

typedef QPair<int, QString> ContactDetails;

static void addEvents(int from, int to)
{
    EventModel eventsModel;
    watcher.setModel(&eventsModel);

    // Since 1.7.0 the RecentContactsModel is sorted by end time instead
    // of start time. Make sure the assumptions in the unit tests hold
    // by adjusting the start times for events with duration (CallEvent)
    // so that their end times are all in the expected order.

    // Wait briefly after adding IM and SMS events, otherwise the additions are not seen by the model.

    QDateTime dateTime = QDateTime::currentDateTime().addDays(-1);
    auto nextTestDateTime = [&dateTime] () {
        dateTime = dateTime.addSecs(1000);
        return dateTime;
    };

    if (to >= 1 && from <= 1) {
        addTestEvent(eventsModel, Event::CallEvent, Event::Inbound, phoneAccount, -1, "", false, false, nextTestDateTime().addSecs(-TESTCALL_SECS), alicePhone1);
    }
    if (to >= 2 && from <= 2) {
        addTestEvent(eventsModel, Event::CallEvent, Event::Outbound, phoneAccount, -1, "", false, false, nextTestDateTime().addSecs(-TESTCALL_SECS), bobPhone);
    }
    if (to >= 3 && from <= 3) {
        addTestEvent(eventsModel, Event::IMEvent, Event::Inbound, charlieIm1.first, group1.id(), "", false, false, nextTestDateTime(), charlieIm1.second);
        QTest::qWait(100);
    }
    if (to >= 4 && from <= 4) {
        addTestEvent(eventsModel, Event::SMSEvent, Event::Outbound, phoneAccount, group1.id(), "", false, false, nextTestDateTime(), alicePhone2);
        QTest::qWait(100);
    }
    if (to >= 5 && from <= 5) {
        addTestEvent(eventsModel, Event::CallEvent, Event::Inbound, phoneAccount, -1, "", false, false, nextTestDateTime().addSecs(-TESTCALL_SECS), bobPhone);
    }
    if (to >= 6 && from <= 6) {
        addTestEvent(eventsModel, Event::IMEvent, Event::Outbound, charlieIm2.first, group1.id(), "", false, false, nextTestDateTime(), charlieIm2.second);
        QTest::qWait(100);
    }
    if (to >= 7 && from <= 7) {
        addTestEvent(eventsModel, Event::IMEvent, Event::Outbound, bobIm.first, group1.id(), "", false, false, nextTestDateTime(), bobIm.second);
        QTest::qWait(100);
    }

    int count = to - from + 1;
    QVERIFY(watcher.waitForAdded(count, count));
}

static void addEvents(int to)
{
    addEvents(1, to);
}

class InsertionSpy : public QObject
{
    Q_OBJECT

public:
    InsertionSpy(QAbstractItemModel &m, QObject *parent = 0)
        : QObject(parent),
          model(m),
          insertionCount(0)
    {
        QObject::connect(&model, SIGNAL(rowsInserted(const QModelIndex &, int, int)),
                         this, SLOT(rowsInserted(const QModelIndex &, int, int)));
    }

    int count() const { return insertionCount; }

private Q_SLOTS:
    void rowsInserted(const QModelIndex &, int start, int end) { insertionCount += (end - start + 1); }

private:
    QAbstractItemModel &model;
    int insertionCount;
};

class RemovalSpy : public QObject
{
    Q_OBJECT

public:
    RemovalSpy(QAbstractItemModel &m, QObject *parent = 0)
        : QObject(parent),
          model(m),
          removalCount(0)
    {
        QObject::connect(&model, SIGNAL(rowsRemoved(const QModelIndex &, int, int)),
                         this, SLOT(rowsRemoved(const QModelIndex &, int, int)));
    }

    int count() const { return removalCount; }

private Q_SLOTS:
    void rowsRemoved(const QModelIndex &, int start, int end) { removalCount += (end - start + 1); }

private:
    QAbstractItemModel &model;
    int removalCount;
};

void RecentContactsModelTest::initTestCase()
{
    initTestDatabase();
    QVERIFY(QDBusConnection::sessionBus().isConnected());
    ContactChangeListener contactChangeListener;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    srand( QDateTime::currentDateTime().toSecsSinceEpoch() );
#else
    qsrand( QDateTime::currentDateTime().toTime_t() );
#endif

    addTestGroups( group1, group2 );

    aliceId = addTestContact(aliceName, alicePhone1, phoneAccount, &contactChangeListener);
    QVERIFY(addTestContactAddress(aliceId, alicePhone2, phoneAccount));

    bobId = addTestContact(bobName, bobPhone, phoneAccount, &contactChangeListener);
    QVERIFY(addTestContactAddress(bobId, bobIm.second, bobIm.first));

    charlieId = addTestContact(charlieName, charlieIm1.second, charlieIm1.first, &contactChangeListener);
    QVERIFY(addTestContactAddress(charlieId, charlieIm2.second, charlieIm2.first));
}

void RecentContactsModelTest::init()
{
    // Re-add the groups which are purged when all events are removed
    addTestGroups( group1, group2 );
}

void RecentContactsModelTest::simple()
{
    addEvents(3);

    RecentContactsModel model;

    InsertionSpy insert(model);

    QVERIFY(model.getEvents());
    QTRY_COMPARE(model.resolving(), false);
    QCOMPARE(insert.count(), 3);

    // We should have one row for each contact
    QCOMPARE(model.rowCount(), 3);

    Event e;

    // Events must be in reverse chronological order
    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(charlieId, charlieName));
    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    e = model.event(model.index(2, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));
}

void RecentContactsModelTest::limitedFill()
{
    addEvents(3);

    RecentContactsModel model;
    model.setLimit(2);

    InsertionSpy insert(model);

    QVERIFY(model.getEvents());
    QTRY_COMPARE(model.resolving(), false);
    QCOMPARE(insert.count(), 2);

    // We should have one row for each contact, limited to 2
    QCOMPARE(model.rowCount(), 2);

    Event e;

    // Events must be in reverse chronological order
    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(charlieId, charlieName));
    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
}

void RecentContactsModelTest::repeated()
{
    addEvents(5);

    RecentContactsModel model;
    model.setLimit(3);

    InsertionSpy insert(model);

    QVERIFY(model.getEvents());
    QTRY_COMPARE(model.resolving(), false);
    QCOMPARE(insert.count(), 3);

    // We should have one row for each contact
    QCOMPARE(model.rowCount(), 3);

    Event e;

    // Events must be in reverse chronological order
    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    QCOMPARE(e.type(), Event::CallEvent);
    QCOMPARE(e.direction(), Event::Inbound);
    QCOMPARE(e.recipients().count(), 1);
    QCOMPARE(e.recipients().at(0), Recipient(phoneAccount, bobPhone));

    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));
    QCOMPARE(e.type(), Event::SMSEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.recipients().count(), 1);
    QCOMPARE(e.recipients().at(0), Recipient(phoneAccount, alicePhone2));

    e = model.event(model.index(2, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(charlieId, charlieName));
    QCOMPARE(e.type(), Event::IMEvent);
    QCOMPARE(e.direction(), Event::Inbound);
    QCOMPARE(e.recipients().count(), 1);
    QCOMPARE(e.recipients().at(0), Recipient(charlieIm1.first, charlieIm1.second));
}

void RecentContactsModelTest::limitedDynamic()
{
    addEvents(2);

    RecentContactsModel model;
    model.setLimit(2);

    InsertionSpy insert(model);

    QVERIFY(model.getEvents());
    QTRY_COMPARE(model.resolving(), false);
    QCOMPARE(insert.count(), 2);

    // We should have one row for each contact, limited to 2
    QCOMPARE(model.rowCount(), 2);

    Event e;

    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));

    // Add more events; we have to wait for each one to be sure of when we're done
    for (int count = 3; count <= 5; ++count) {
        addEvents(count, count);
        QTRY_COMPARE(model.resolving(), false);
        QCOMPARE(insert.count(), count);
    }

    QTRY_COMPARE(insert.count(), 5);
    QCOMPARE(model.rowCount(), 2);

    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    QCOMPARE(e.type(), Event::CallEvent);
    QCOMPARE(e.direction(), Event::Inbound);
    QCOMPARE(e.recipients().count(), 1);
    QCOMPARE(e.recipients().at(0), Recipient(phoneAccount, bobPhone));

    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));
    QCOMPARE(e.type(), Event::SMSEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.recipients().count(), 1);
    QCOMPARE(e.recipients().at(0), Recipient(phoneAccount, alicePhone2));
}

void RecentContactsModelTest::differentTypes()
{
    RecentContactsModel model;
    model.setLimit(3);

    QVERIFY(model.getEvents());
    QTRY_COMPARE(model.resolving(), false);
    QCOMPARE(model.rowCount(), 0);

    InsertionSpy insert(model);

    for (int count = 1; count <= 7; ++count) {
        addEvents(count, count);
        QTRY_COMPARE(model.resolving(), false);
        QCOMPARE(insert.count(), count);
    }

    // We should have one row for each contact
    QCOMPARE(model.rowCount(), 3);

    Event e;

    // Events must be in reverse chronological order
    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    QCOMPARE(e.type(), Event::IMEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.recipients().count(), 1);
    QCOMPARE(e.recipients().at(0), Recipient(bobIm.first, bobIm.second));

    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(charlieId, charlieName));
    QCOMPARE(e.type(), Event::IMEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.recipients().count(), 1);
    QCOMPARE(e.recipients().at(0), Recipient(charlieIm2.first, charlieIm2.second));

    e = model.event(model.index(2, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));
    QCOMPARE(e.type(), Event::SMSEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.recipients().count(), 1);
    QCOMPARE(e.recipients().at(0), Recipient(phoneAccount, alicePhone2));
}

void RecentContactsModelTest::categoryMask_data()
{
    QTest::addColumn<int>("categoryMask");
    QTest::addColumn<QList<int> >("expectedContactIds");

    QTest::newRow("any")
        << static_cast<int>(Event::AnyCategory)
        << (QList<int>() << bobId << charlieId << aliceId);
    QTest::newRow("voicecall")
        << static_cast<int>(Event::VoicecallCategory)
        << (QList<int>() << bobId << aliceId);
    QTest::newRow("voicemail")
        << static_cast<int>(Event::VoicemailCategory)
        << QList<int>();
    QTest::newRow("sms")
        << static_cast<int>(Event::ShortMessagingCategory)
        << (QList<int>() << aliceId);
    QTest::newRow("instant")
        << static_cast<int>(Event::InstantMessagingCategory)
        << (QList<int>() << bobId << charlieId);
    QTest::newRow("sms or instant")
        << static_cast<int>(Event::ShortMessagingCategory | Event::InstantMessagingCategory)
        << (QList<int>() << bobId << charlieId << aliceId);
    QTest::newRow("sms or voicecall")
        << static_cast<int>(Event::ShortMessagingCategory | Event::VoicecallCategory)
        << (QList<int>() << bobId << aliceId);
}

void RecentContactsModelTest::categoryMask()
{
    QFETCH(int, categoryMask);
    QFETCH(QList<int>, expectedContactIds);

    RecentContactsModel model;
    model.setLimit(7);
    model.setEventCategoryMask(categoryMask);

    QVERIFY(model.getEvents());
    QTRY_COMPARE(model.resolving(), false);
    QCOMPARE(model.rowCount(), 0);

    addEvents(1, 7);
    QTRY_COMPARE(model.resolving(), false);

    QList<int> eventContactIds;
    for (int i = 0; i < model.rowCount(); ++i) {
        Event e = model.event(model.index(i, 0));
        eventContactIds.append(e.recipients().at(0).contactId());
    }
    QCOMPARE(eventContactIds, expectedContactIds);

    // Test the same result at population
    QVERIFY(model.getEvents());
    QTRY_COMPARE(model.resolving(), false);

    eventContactIds.clear();
    for (int i = 0; i < model.rowCount(); ++i) {
        Event e = model.event(model.index(i, 0));
        eventContactIds.append(e.recipients().at(0).contactId());
    }
    QCOMPARE(eventContactIds, expectedContactIds);
}

void RecentContactsModelTest::requiredProperty_data()
{
    QTest::addColumn<bool>("addEventsBeforeModels");
    QTest::newRow("add events before model creation") << true;
    QTest::newRow("add events after model creation") << true;
}

void RecentContactsModelTest::requiredProperty()
{
    QFETCH(bool, addEventsBeforeModels);

    if (addEventsBeforeModels) {
        addEvents(1, 7);
    }

    RecentContactsModel phoneModel;
    phoneModel.setRequiredProperty(RecentContactsModel::PhoneNumberRequired);

    RecentContactsModel imModel;
    imModel.setRequiredProperty(RecentContactsModel::AccountUriRequired);

    RecentContactsModel emailModel;
    emailModel.setRequiredProperty(RecentContactsModel::EmailAddressRequired);

    RecentContactsModel phoneAndImModel;
    phoneAndImModel.setRequiredProperty(RecentContactsModel::PhoneNumberRequired | RecentContactsModel::AccountUriRequired);

    QVERIFY(phoneModel.getEvents());
    QVERIFY(imModel.getEvents());
    QVERIFY(emailModel.getEvents());
    QVERIFY(phoneAndImModel.getEvents());

    QCOMPARE(phoneModel.rowCount(), 0);
    QCOMPARE(imModel.rowCount(), 0);
    QCOMPARE(emailModel.rowCount(), 0);
    QCOMPARE(phoneAndImModel.rowCount(), 0);

    if (!addEventsBeforeModels) {
        addEvents(1, 7);
    }

    QTRY_COMPARE(phoneModel.resolving(), false);
    QTRY_COMPARE(imModel.resolving(), false);
    QTRY_COMPARE(emailModel.resolving(), false);
    QTRY_COMPARE(phoneAndImModel.resolving(), false);

    // Two contacts have phone numbers, two have IM addresses, none have email
    QCOMPARE(phoneModel.rowCount(), 2);
    QCOMPARE(imModel.rowCount(), 2);
    QCOMPARE(emailModel.rowCount(), 0);
    QCOMPARE(phoneAndImModel.rowCount(), 3);

    Event e;

    // Phone contacts (not events)
    e = phoneModel.event(phoneModel.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    QCOMPARE(e.type(), Event::IMEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.recipients().count(), 1);
    QCOMPARE(e.recipients().at(0), Recipient(bobIm.first, bobIm.second));

    e = phoneModel.event(phoneModel.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));
    QCOMPARE(e.type(), Event::SMSEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.recipients().count(), 1);
    QCOMPARE(e.recipients().at(0), Recipient(phoneAccount, alicePhone2));

    // IM contacts
    e = imModel.event(imModel.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    QCOMPARE(e.type(), Event::IMEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.recipients().count(), 1);
    QCOMPARE(e.recipients().at(0), Recipient(bobIm.first, bobIm.second));

    e = imModel.event(imModel.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(charlieId, charlieName));
    QCOMPARE(e.type(), Event::IMEvent);
    QCOMPARE(e.direction(), Event::Outbound);
    QCOMPARE(e.recipients().count(), 1);
    QCOMPARE(e.recipients().at(0), Recipient(charlieIm2.first, charlieIm2.second));
}

void RecentContactsModelTest::contactRemoved()
{
    QString dougalName("Dougal");
    QString dougalPhone("5550000");
    ContactChangeListener contactChangeListener;
    int dougalId = addTestContact(dougalName, dougalPhone, phoneAccount, &contactChangeListener);

    addEvents(2);

    // Add an event for the new contact
    EventModel eventsModel;
    watcher.setModel(&eventsModel);
    addTestEvent(eventsModel, Event::CallEvent, Event::Inbound, phoneAccount, -1, "", false, false, QDateTime::currentDateTime(), dougalPhone);
    QVERIFY(watcher.waitForAdded());

    RecentContactsModel model;
    InsertionSpy insert(model);
    RemovalSpy remove(model);

    QVERIFY(model.getEvents());
    QTRY_COMPARE(model.resolving(), false);
    QCOMPARE(insert.count(), 3);

    // We should have one row for each contact
    QCOMPARE(model.rowCount(), 3);

    Event e;

    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(dougalId, dougalName));
    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    e = model.event(model.index(2, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));

    QCOMPARE(remove.count(), 0);
    deleteTestContact(dougalId, &contactChangeListener);

    // The removed contact's event should be removed
    QTRY_COMPARE(remove.count(), 1);

    QCOMPARE(model.rowCount(), 2);

    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));

    // Reset the model, the removed contact should still be absent
    QVERIFY(model.getEvents());
    QTRY_COMPARE(model.resolving(), false);
    QCOMPARE(model.rowCount(), 2);

    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));
}

void RecentContactsModelTest::favoritesExcluded()
{
    QString dougalName("Dougal");
    QString dougalPhone("5550000");
    ContactChangeListener contactChangeListener;
    int dougalId = addTestContact(dougalName, dougalPhone, phoneAccount, &contactChangeListener);

    addEvents(2);

    // Add an event for the new contact
    EventModel eventsModel;
    watcher.setModel(&eventsModel);
    addTestEvent(eventsModel, Event::CallEvent, Event::Inbound, phoneAccount, -1, "", false, false, QDateTime::currentDateTime(), dougalPhone);
    QVERIFY(watcher.waitForAdded());

    RecentContactsModel model;
    InsertionSpy insert(model);
    RemovalSpy removal(model);

    model.setExcludeFavorites(true);
    QVERIFY(model.getEvents());
    QTRY_COMPARE(model.resolving(), false);
    QCOMPARE(insert.count(), 3);

    // We should have one row for each contact
    QCOMPARE(model.rowCount(), 3);

    Event e;

    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(dougalId, dougalName));
    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    e = model.event(model.index(2, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));

    // Mark the contact as a favorite
    QCOMPARE(removal.count(), 0);
    modifyTestContact(dougalId, dougalName, true);

    // The favorite contact's event should be removed
    QTRY_COMPARE(removal.count(), 1);

    QCOMPARE(model.rowCount(), 2);

    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));

    // Reset the model, the favorite contact should still be absent
    QVERIFY(model.getEvents());
    QTRY_COMPARE(model.resolving(), false);
    QCOMPARE(model.rowCount(), 2);

    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));

    // Reset without exclusion
    model.setExcludeFavorites(false);
    QVERIFY(model.getEvents());
    QTRY_COMPARE(model.resolving(), false);
    QCOMPARE(model.rowCount(), 3);

    e = model.event(model.index(0, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(dougalId, dougalName));
    e = model.event(model.index(1, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(bobId, bobName));
    e = model.event(model.index(2, 0));
    QCOMPARE(e.contacts(), QList<ContactDetails>() << qMakePair(aliceId, aliceName));
}

void RecentContactsModelTest::cleanup()
{
    deleteAll(false);
}

void RecentContactsModelTest::cleanupTestCase()
{
    deleteAll();
}

QTEST_MAIN(RecentContactsModelTest)

#include "recentcontactsmodeltest.moc"
