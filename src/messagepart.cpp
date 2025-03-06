/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2014 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: John Brooks <john.brooks@jolla.com>
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

#include <QSharedDataPointer>
#include <QDBusArgument>
#include <QStringBuilder>
#include <QFile>
#include <QFileInfo>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#endif
#include <QDataStream>
#include "messagepart.h"
#include "debug_p.h"

namespace CommHistory {

class MessagePartPrivate : public QSharedData
{
public:
    MessagePartPrivate();
    MessagePartPrivate(const MessagePartPrivate &other);
    ~MessagePartPrivate();

    int id;
    QString contentId;
    QString contentType;
    QString path;
};

}

using namespace CommHistory;

QDBusArgument &operator<<(QDBusArgument &argument, const MessagePart &part)
{
    argument.beginStructure();
    argument << part.id() << part.contentId() << part.contentType() << part.path();
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, MessagePart &part)
{
    int id;
    QString contentId, contentType, path;
    argument.beginStructure();
    argument >> id >> contentId >> contentType >> path;
    argument.endStructure();
    part.setId(id);
    part.setContentId(contentId);
    part.setContentType(contentType);
    part.setPath(path);
    return argument;
}

QDataStream &operator<<(QDataStream &stream, const CommHistory::MessagePart &part)
{
    stream << part.id() << part.contentId() << part.contentType() << part.path();
    return stream;
}

QDataStream &operator>>(QDataStream &stream, CommHistory::MessagePart &part)
{
    stream >> part.d->id >> part.d->contentId >> part.d->contentType >> part.d->path;
    return stream;
}

MessagePartPrivate::MessagePartPrivate()
    : id(-1)
{
}

MessagePartPrivate::MessagePartPrivate(const MessagePartPrivate &other)
    : QSharedData(other)
    , id(other.id)
    , contentId(other.contentId)
    , contentType(other.contentType)
    , path(other.path)
{
}

MessagePartPrivate::~MessagePartPrivate()
{
}

MessagePart::MessagePart()
    : d(new MessagePartPrivate)
{
}

MessagePart::MessagePart(const MessagePart &other)
    : d(other.d)
{
}

MessagePart &MessagePart::operator=(const MessagePart &other)
{
    d = other.d;
    return *this;
}

bool MessagePart::operator==(const MessagePart &other) const
{
    return (d->id == other.id()
            && d->contentId == other.contentId()
            && d->contentType == other.contentType()
            && d->path == other.path());
}

MessagePart::~MessagePart()
{
}

int MessagePart::id() const
{
    return d->id;
}

QString MessagePart::contentId() const
{
    return d->contentId;
}

QString MessagePart::contentType() const
{
    return d->contentType;
}

QString MessagePart::path() const
{
    return d->path;
}

void MessagePart::setId(int id)
{
    d->id = id;
}

void MessagePart::setContentId(const QString &contentId)
{
    d->contentId = contentId;
}

void MessagePart::setContentType(const QString &type)
{
    d->contentType = type;
}

void MessagePart::setPath(const QString &path)
{
    d->path = path;
}

int MessagePart::size() const
{
    return QFileInfo(d->path).size();
}

QString MessagePart::plainTextContent() const
{
    if (!d->contentType.startsWith("text/"))
        return QString();

    QFile file(d->path);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcCommHistory) << "Message part" << id() << "at" << path() << "can't be read";
        return QString();
    }

    QByteArray content = file.readAll();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QTextCodec *codec = 0;
#endif
    int pos = d->contentType.indexOf(";charset=");
    if (pos > 0) {
        pos += strlen(";charset=");
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QStringRef charset = d->contentType.midRef(pos, d->contentType.indexOf(';', pos) - pos).trimmed();
        codec = QTextCodec::codecForName(charset.toLatin1());
        if (!codec)
            qCWarning(lcCommHistory) << "Missing text codec for" << charset << "when parsing content of type" << d->contentType;
#endif
    }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (codec)
        return codec->toUnicode(content);
    else
#endif
        return QString::fromUtf8(content);
}

QString MessagePart::debugString() const
{
    return QString(QString::number(id())   % QChar('|') %
                   contentId()             % QChar('|') %
                   contentType()           % QChar('|') %
                   path());
}
