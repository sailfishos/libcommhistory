#include "callproxymodel.h"
#include "event.h"
#include "eventmodel.h"

using namespace CommHistory;

CallProxyModel::CallProxyModel(QObject *parent) :
    QSortFilterProxyModel(parent),
    m_source(new CommHistory::CallModel(this)),
    m_grouping(GroupByNone),
    m_limit(0),
    m_resolveContacts(false),
    m_bufferInsertions(false),
    m_componentComplete(false),
    m_populated(false)
{
    m_source->setQueryMode(CommHistory::EventModel::AsyncQuery);
    m_source->setFilter(CommHistory::CallModel::Sorting(m_grouping));
    m_source->setLimit(m_limit);
    m_source->setResolveContacts(m_resolveContacts ? EventModel::ResolveImmediately : EventModel::ResolveOnDemand);
    m_source->setBufferInsertions(m_bufferInsertions);
}

void CallProxyModel::classBegin()
{
}

void CallProxyModel::componentComplete()
{
    m_componentComplete = true;

    this->setSourceModel(m_source);
    this->setDynamicSortFilter(true);

    connect(this, SIGNAL(rowsInserted(const QModelIndex&,int,int)), this, SIGNAL(countChanged()));
    connect(this, SIGNAL(rowsRemoved(const QModelIndex&,int,int)), this, SIGNAL(countChanged()));
    connect(this, SIGNAL(modelReset()), this, SIGNAL(countChanged()));

    connect(m_source, SIGNAL(modelReady(bool)), this, SLOT(modelReady(bool)));

    if (!m_source->getEvents()) {
        qWarning() << "getEvents() failed on CommHistory::CallModel";
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

        m_source->setFilter(CommHistory::CallModel::Sorting(grouping));
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

bool CallProxyModel::bufferInsertions() const
{
    return m_bufferInsertions;
}

void CallProxyModel::setBufferInsertions(bool buffer)
{
    if (m_bufferInsertions != buffer) {
        m_bufferInsertions = buffer;

        m_source->setBufferInsertions(buffer);
        emit bufferInsertionsChanged();
    }
}

void CallProxyModel::setLimit(int count)
{
    if (count != m_limit) {
        m_limit = count;

        m_source->setLimit(count);
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

        m_source->setResolveContacts(m_resolveContacts ? EventModel::ResolveImmediately : EventModel::ResolveOnDemand);
        if (m_componentComplete && m_resolveContacts) {
            // Model must be reloaded to resolve contacts if getEvents was already called
            m_source->getEvents();
        }

        emit resolveContactsChanged();
    }
}

void CallProxyModel::setSortRole(int role)
{
    QSortFilterProxyModel::setSortRole(role);
}

void CallProxyModel::setFilterRole(int role)
{
    QSortFilterProxyModel::setFilterKeyColumn(role - CommHistory::EventModel::BaseRole);
    QSortFilterProxyModel::setFilterRole(role);
}

void CallProxyModel::deleteAt(int index)
{
    QModelIndex sourceIndex = mapToSource(CallProxyModel::index(index, 0));
    CommHistory::Event event = m_source->event(sourceIndex);
    if (event.isValid()) {
        m_source->deleteEvent(event);
    }
}

bool CallProxyModel::markAllRead()
{
    return m_source->markAllRead();
}

bool CallProxyModel::populated() const
{
    return m_populated;
}

void CallProxyModel::modelReady(bool ready)
{
    if (ready) {
        m_populated = true;
        emit populatedChanged();
    }
}

