/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2020 D. Caliste
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

#ifndef COMMHISTORY_RECIPIENT_EVENT_MODEL_H
#define COMMHISTORY_RECIPIENT_EVENT_MODEL_H

#include "libcommhistoryexport.h"
#include "eventmodel.h"

namespace CommHistory {

class RecipientEventModelPrivate;

/*!
 * \class RecipientEventModel
 * \brief Model representing events from one or several local / remote uids.
 */
class LIBCOMMHISTORY_EXPORT RecipientEventModel : public EventModel
{
    Q_OBJECT

public:
    /*!
     * Model constructor.
     *
     * \param parent Parent object.
     */
    RecipientEventModel(QObject *parent = 0);

    /*!
     * Destructor.
     */
    ~RecipientEventModel();

    /*!
     * Populate model with events matching the recipient list.
     * It's not necessary that the recipients are contact resolved.
     *
     * \param recipients, local / remote uid pairs to be fetched.
     *
     * \return true if successful, otherwise false
     */
    bool getEvents(const RecipientList &recipients);
    /*!
     * Populate model with events matching the given recipient.
     * It's not necessary that the recipient is contact resolved.
     *
     * \param recipient, local / remote uid pairs to be fetched.
     *
     * \return true if successful, otherwise false
     */
    bool getEvents(const Recipient &recipient);

protected:
    RecipientEventModel(RecipientEventModelPrivate &dd, QObject *parent = 0);

private:
    Q_DECLARE_PRIVATE(RecipientEventModel)
};

} // namespace CommHistory

#endif // COMMHISTORY_RECIPIENT_EVENT_MODEL_H

