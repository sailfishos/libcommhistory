/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2020 D. Caliste
** Copyright (C) 2020 Open Mobile Platform LLC.
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
 * \brief Model representing events from one or several local / remote uids or contacts.
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
     * Set the recipient for the model.
     * The model will attempt to resolve it to a contact when fetching events
     * for this recipient. If it does not resolve to a contact, the recipient
     * the model will just fetch events for this recipient.
     *
     * \param recipient, local / remote uid pair to be fetched.
     */
    void setRecipients(const Recipient &recipient);

    /*!
     * Sets the recipients for the model.
     *
     * \overload
     */
    void setRecipients(const RecipientList &recipients);

    /*!
     * Set the recipients for the model using a contact ID.
     * The model will fetch events for the recipients of this contact.
     * For example, if a contact has multiple phone numbers, all of
     * its phone numbers will be added as recipients to the model.
     *
     * \param contactId ID of the contact to be fetched.
     */
    void setRecipients(int contactId);

    /*!
     * Populate the model with events matching the specified recipients.
     *
     * \param recipients, local / remote uid pairs to be fetched.
     *
     * \return true if successful, otherwise false
     */
    bool getEvents();

protected:
    RecipientEventModel(RecipientEventModelPrivate &dd, QObject *parent = 0);

private:
    Q_DECLARE_PRIVATE(RecipientEventModel)
};

} // namespace CommHistory

#endif // COMMHISTORY_RECIPIENT_EVENT_MODEL_H

