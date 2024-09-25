#include "callproxymodel.h"
#include "event.h"
#include "eventmodel.h"
#include "debug.h"

using namespace CommHistory;

CallProxyModel::CallProxyModel(QObject *parent) :
    CommHistory::CallModel(parent),
    m_grouping(GroupByNone),
    m_limit(0),
    m_resolveContacts(false),
    m_componentComplete(false),
    m_populated(false)
{
    setQueryMode(CommHistory::EventModel::AsyncQuery);
    setFilter(CommHistory::CallModel::Sorting(m_grouping));
    setLimit(m_limit);
    setResolveContacts(m_resolveContacts ? EventModel::ResolveImmediately : EventModel::ResolveOnDemand);
}

void CallProxyModel::classBegin()
{
}

void CallProxyModel::componentComplete()
{
    m_componentComplete = true;

    connect(this, SIGNAL(rowsInserted(const QModelIndex&,int,int)), this, SIGNAL(countChanged()));
    connect(this, SIGNAL(rowsRemoved(const QModelIndex&,int,int)), this, SIGNAL(countChanged()));
    connect(this, SIGNAL(modelReset()), this, SIGNAL(countChanged()));

    connect(this, SIGNAL(modelReady(bool)), this, SLOT(onReadyChanged(bool)));

    if (!getEvents()) {
        qCWarning(lcCommHistory) << "getEvents() failed on CommHistory::CallModel";
    }
}

CallProxyModel::GroupBy CallProxyModel::groupBy() const
{
    return m_grouping;
}

void CallProxyModel::setGroupBy(GroupBy grouping)
{
    if (m_grouping != grouping) {
        m_grouping = grouping;

        setFilter(CommHistory::CallModel::Sorting(grouping));
        emit groupByChanged();
    }
}

int CallProxyModel::count() const
{
    return rowCount();
}

int CallProxyModel::limit() const
{
    return m_limit;
}

void CallProxyModel::setLimit(int count)
{
    if (count != m_limit) {
        m_limit = count;

        CallModel::setLimit(count);
        emit limitChanged();
    }
}

bool CallProxyModel::resolveContacts() const
{
    return m_resolveContacts;
}

void CallProxyModel::setResolveContacts(bool enabled)
{
    if (m_resolveContacts != enabled) {
        m_resolveContacts = enabled;

        CallModel::setResolveContacts(m_resolveContacts ? EventModel::ResolveImmediately : EventModel::ResolveOnDemand);
        if (m_componentComplete && m_resolveContacts) {
            // Model must be reloaded to resolve contacts if getEvents was already called
            getEvents();
        }

        emit resolveContactsChanged();
    }
}

void CallProxyModel::deleteAt(int index)
{
    CommHistory::Event e = event(CallModel::index(index, 0));
    if (e.isValid()) {
        deleteEvent(e);
    }
}

bool CallProxyModel::markAllRead()
{
    return CallModel::markAllRead();
}

bool CallProxyModel::populated() const
{
    return m_populated;
}

void CallProxyModel::onReadyChanged(bool ready)
{
    if (ready) {
        m_populated = true;
        emit populatedChanged();
    }
}

int CallProxyModel::createOutgoingCallEvent(const QString &localUid, const QString &remoteUid)
{
    Event event;
    EventModel model;

    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(event.startTime());
    event.setType(CommHistory::Event::CallEvent);
    event.setDirection(CommHistory::Event::Outbound);
    event.setLocalUid(localUid);
    event.setRecipients(Recipient(localUid, remoteUid));
    if (model.addEvent(event))
        return event.id();
    return -1;
}
