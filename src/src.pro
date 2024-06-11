###############################################################################
#
# This file is part of libcommhistory.
#
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
# Contact: Reto Zingg <reto.zingg@nokia.com>
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License version 2.1 as
# published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
#
###############################################################################

!include( ../common-project-config.pri ) : \
    error( "Unable to include common-project-config.pri" )
!include( ../common-vars.pri ) : \
    error( "Unable to include common-vars.pri" )
!include( ../common-installs-config.pri ) : \
    error( "Unable to include common-installs-config.pri" )


# -----------------------------------------------------------------------------
# target setup
# -----------------------------------------------------------------------------
TEMPLATE = lib
VERSION  = $$LIBRARY_VERSION

CONFIG  += shared \
           no_install_prl \
           debug

QT += dbus sql contacts
QT -= gui

TARGET = commhistory-qt5
PKGCONFIG += qtcontacts-sqlite-qt5-extensions contactcache-qt5
LIBS += -lphonenumber

DEFINES += LIBCOMMHISTORY_SHARED
CONFIG += hide_symbols

# -----------------------------------------------------------------------------
# input
# -----------------------------------------------------------------------------
QT_LIKE_HEADERS += \
                   headers/CallEvent \
                   headers/CallModel \
                   headers/CallStatistics \
                   headers/SmsHistory \
                   headers/CallHistory \
                   headers/ContactListener \
                   headers/ContactResolver \
                   headers/ConversationModel \
                   headers/Event \
                   headers/EventModel \
                   headers/MessagePart \
                   headers/Group \
                   headers/GroupModel \
                   headers/SingleEventModel \
                   headers/RecentContactsModel \
                   headers/Recipient \
                   headers/Events \
                   headers/Models \
                   headers/DatabaseIO

PUBLIC_HEADERS += \
           commonutils.h \
           eventmodel.h \
           event.h \
           messagepart.h \
           callevent.h \
           conversationmodel.h \
           callstatistics.h \
           smshistory.h \
           callhistory.h \
           callmodel.h \
           groupmodel.h \
           group.h \
           updateslistener.h \
           contactlistener.h \
           libcommhistoryexport.h \
           singleeventmodel.h \
           recipienteventmodel.h \
           recentcontactsmodel.h \
           mmsconstants.h \
           mmsreadreportmodel.h \
           groupobject.h \
           groupmanager.h \
           contactgroupmodel.h \
           contactgroup.h \
           databaseio.h \
           commhistorydatabasepath.h \
           contactfetcher.h \
           contactresolver.h \
           draftsmodel.h \
           recipient.h

HEADERS += \
           $$PUBLIC_HEADERS \
           commhistorydatabase.h \
           adaptor.h \
           dbus_p.h \
           debug.h \
           eventmodel_p.h \
           eventtreeitem.h \
           callstatistics_p.h \
           smshistory_p.h \
           callhistory_p.h \
           groupmodel_p.h \
           conversationmodel_p.h \
           recipienteventmodel_p.h \
           updatesemitter.h \
           databaseio_p.h \
           draftsmodel_p.h \

SOURCES += commonutils.cpp \
           eventmodel.cpp \
           eventmodel_p.cpp \
           eventtreeitem.cpp \
           conversationmodel.cpp \
           callstatistics.cpp \
           callhistory.cpp \
           callmodel.cpp \
           groupmodel.cpp \
           group.cpp \
           adaptor.cpp \
           event.cpp \
           messagepart.cpp \
           mmsreadreportmodel.cpp \
           contactlistener.cpp \
           smshistory.cpp \
           singleeventmodel.cpp \
           recipienteventmodel.cpp \
           recentcontactsmodel.cpp \
           updatesemitter.cpp \
           updateslistener.cpp \
           groupmanager.cpp \
           groupobject.cpp \
           contactgroupmodel.cpp \
           contactgroup.cpp \
           databaseio.cpp \
           commhistorydatabase.cpp \
           contactfetcher.cpp \
           contactresolver.cpp \
           draftsmodel.cpp \
           recipient.cpp

# -----------------------------------------------------------------------------
# Installation target for API header files
# -----------------------------------------------------------------------------
headers.files = $$PUBLIC_HEADERS \
                $$QT_LIKE_HEADERS

headers.path = $${INSTALL_PREFIX}/include/commhistory-qt5/CommHistory

# -----------------------------------------------------------------------------
# Installation target for .pc file
# -----------------------------------------------------------------------------
QMAKE_SUBSTITUTES += $${TARGET}.pc.in
pkgconfig.files = $${TARGET}.pc
pkgconfig.path  = $$[QT_INSTALL_LIBS]/pkgconfig

target.path  = $$[QT_INSTALL_LIBS]

INSTALLS += pkgconfig target headers
