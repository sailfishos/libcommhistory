include( ../common-project-config.pri )
include( ../common-vars.pri )

TARGET = commhistory-declarative
PLUGIN_IMPORT_PATH = org/nemomobile/commhistory
VERSION = $$PROJECT_VERSION

INCLUDEPATH += ../src

SOURCES += src/plugin.cpp \
    src/callproxymodel.cpp \
    src/declarativerecipienteventmodel.cpp \
    src/conversationproxymodel.cpp \
    src/declarativegroupmanager.cpp \
    src/sharedbackgroundthread.cpp \
    src/debug.cpp \
    src/mmshelper.cpp \
    src/draftevent.cpp

HEADERS += src/constants.h \
    src/callproxymodel.h \
    src/declarativerecipienteventmodel.h \
    src/conversationproxymodel.h \
    src/declarativegroupmanager.h \
    src/sharedbackgroundthread.h \
    src/debug.h \
    src/plugin.h \
    src/mmshelper.h \
    src/draftevent.h

TEMPLATE = lib
CONFIG += qt plugin hide_symbols
QT += qml contacts dbus
QT -= gui

LIBS += -L../src ../src/libcommhistory-qt$${QT_MAJOR_VERSION}.so
PKGCONFIG += qtcontacts-sqlite-qt$${QT_MAJOR_VERSION}-extensions contactcache-qt$${QT_MAJOR_VERSION}

target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += qmldir plugins.qmltypes
qmldir.path +=  $$target.path
INSTALLS += qmldir

qmltypes.commands = qmlplugindump -nonrelocatable org.nemomobile.commhistory 1.0 > $$PWD/plugins.qmltypes
QMAKE_EXTRA_TARGETS += qmltypes
