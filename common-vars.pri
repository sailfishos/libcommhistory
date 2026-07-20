###############################################################################
#
# This file is part of libcommhistory.
#
# Copyright (C) 2013-2019 Jolla Ltd.
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#------------------------------------------------------------------------------
# Common variables for all projects.
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# Project name (used e.g. in include file and doc install path).
#------------------------------------------------------------------------------
PROJECT_NAME = libcommhistory-qt$${QT_MAJOR_VERSION}

#------------------------------------------------------------------------------
# Project version
#-----------------------------------------------------------------------------
# This should be passed on qmake command line
isEmpty(PROJECT_VERSION) {
    PROJECT_VERSION = 1.9.44
    message("PROJECT_VERSION is unset, assuming $$PROJECT_VERSION")
}

# This should be passed on qmake command line
isEmpty(PKGCONFIG_LIB) {
    PKGCONFIG_LIB = lib
    message("PKGCONFIG_LIB is unset, assuming $$PKGCONFIG_LIB")
}

#------------------------------------------------------------------------------
# Library version
#------------------------------------------------------------------------------
LIBRARY_VERSION = $$PROJECT_VERSION

# End of File
