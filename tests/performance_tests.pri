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

QT          += testlib dbus
TEMPLATE     = app
INCLUDEPATH += . ../../src ..
DEPENDPATH  += $${INCLUDEPATH}

LIBS += ../../src/libcommhistory-qt$${QT_MAJOR_VERSION}.so
QT += contacts
PKGCONFIG += qtcontacts-sqlite-qt$${QT_MAJOR_VERSION}-extensions contactcache-qt$${QT_MAJOR_VERSION}

SOURCES += ../common.cpp
HEADERS += ../common.h

DEFINES += PERF_ITERATIONS=5
DEFINES += PERF_BATCH_SIZE=25

target.path = /opt/tests/$${PROJECT_NAME}/performance
