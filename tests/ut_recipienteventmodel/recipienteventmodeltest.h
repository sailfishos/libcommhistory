/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2020 D. Caliste.
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

#ifndef RECIPIENTEVENTMODELTEST_H
#define RECIPIENTEVENTMODELTEST_H

#include <QObject>

class RecipientEventModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testGetRecipientEvents_data();
    void testGetRecipientEvents();

    void testGetContactEvents_data();
    void testGetContactEvents();

    void testLimitOffset_data();
    void testLimitOffset();
};

#endif
