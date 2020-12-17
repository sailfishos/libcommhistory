/* Copyright (C) 2014-2017 Jolla Ltd.
 * Contact: John Brooks <john.brooks@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "mmshelper.h"
#include "mmsconstants.h"
#include "singleeventmodel.h"
#include <QtDBus>
#include <QTextCodec>
#include <QTemporaryDir>
#include <QMimeDatabase>
#include <QDebug>

struct MmsPart
{
    QString fileName;
    QString contentType;
    QString contentId;
};

inline QDBusArgument& operator<<(QDBusArgument &arg, const MmsPart &part)
{
    arg.beginStructure();
    arg << part.fileName << part.contentType << part.contentId;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument& operator>>(const QDBusArgument &arg, MmsPart &part)
{
    arg.beginStructure();
    arg >> part.fileName >> part.contentType >> part.contentId;
    arg.endStructure();
    return arg;
}

typedef QList<MmsPart> MmsPartList;

Q_DECLARE_METATYPE(MmsPart)
Q_DECLARE_METATYPE(MmsPartList)

using namespace CommHistory;

// QObject wrapper around QTemporaryDir to keep the temporary directory around
// until the D-Bus call completes
class MmsHelper::TempDir : public QObject {
    Q_OBJECT

public:
    TempDir(QObject *parent = nullptr)
        : QObject(parent)
        , m_tempDir(TempDir::basePath() + "/mms")
    {
    }

    static QString basePath() {
        return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/commhistory-tmp");
    }

    bool isValid() const { return m_tempDir.isValid(); }
    QString path() const { return m_tempDir.path(); }

private:
    QTemporaryDir m_tempDir;
};

MmsHelper::MmsHelper(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<MmsPart>();
    qDBusRegisterMetaType<MmsPartList>();
}

void MmsHelper::callEngine(const QString &method, const QVariantList &args)
{
    QDBusMessage call(QDBusMessage::createMethodCall(MMS_ENGINE_SERVICE, MMS_ENGINE_PATH, MMS_ENGINE_INTERFACE, method));
    call.setArguments(args);
    MMS_ENGINE_BUS.asyncCall(call);
}

bool MmsHelper::receiveMessage(int id)
{
    Event event;
    SingleEventModel model;
    if (model.getEventById(id))
        event = model.event();

    if (!event.isValid()) {
        qWarning() << "MmsHelper::receiveMessage called for unknown event id" << id;
        return false;
    }

    QString imsi = event.subscriberIdentity();
    QByteArray pushData = QByteArray::fromBase64(event.extraProperty(MMS_PROPERTY_PUSH_DATA).toByteArray());

    if (imsi.isEmpty() || pushData.isEmpty()) {
        qWarning() << "MmsHelper::receivedMessage called for event" << id << "without notification data";
        event.setStatus(Event::PermanentlyFailedStatus);
        model.modifyEvent(event);
        return false;
    }

    event.setStatus(Event::WaitingStatus);
    model.modifyEvent(event);

    callEngine("receiveMessage", QVariantList() << id << imsi << true << pushData);
    return true;
}

bool MmsHelper::cancel(int id)
{
    Event event;
    SingleEventModel model;
    if (model.getEventById(id))
        event = model.event();

    if (!event.isValid()) {
        qWarning() << "MmsHelper::cancel called for unknown event id" << id;
        return false;
    }

    if (event.status() != Event::DownloadingStatus && event.status() != Event::WaitingStatus && event.status() != Event::SendingStatus)
        return false;

    callEngine("cancel", QVariantList() << id);

    if (event.direction() == Event::Inbound)
        event.setStatus(Event::ManualNotificationStatus);
    else
        event.setStatus(Event::TemporarilyFailedStatus);
    return model.modifyEvent(event);
}

static QString createTemporaryTextFile(const QString &dir, const QMimeDatabase &mimeDb,
    const QString &text, const QString &contentId, QString &contentType)
{
    QString codec(QStringLiteral("utf-8"));
    QByteArray data = text.toUtf8();

    if (contentType.isEmpty())
        contentType = QStringLiteral("text/plain");
    else
        contentType = contentType.left(contentType.indexOf(';'));

    // QMimeDatabase doesn't like "text/x-vCard" but is happy with "text/x-vcard"
    QMimeType mimeType = mimeDb.mimeTypeForName(contentType.toLower());
    contentType += QStringLiteral(";charset=") + codec;

    QFileInfo fileInfo(dir, contentId);
    QString path = fileInfo.absoluteFilePath();
    if (mimeType.isValid()) {
        // Make sure that file has the right suffix
        QString ext = fileInfo.suffix();
        if (ext.isEmpty() || !mimeType.suffixes().contains(ext, Qt::CaseInsensitive)) {
            // Append the default extension
            path += "." + mimeType.preferredSuffix();
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "MmsHelper::sendMessage: failed to open" << qPrintable(path);
        return QString();
    }

    if (file.write(data) < data.size()) {
        qWarning() << "MmsHelper::sendMessage: failed to write" << qPrintable(path);
        return QString();
    }

    return file.fileName();
}

bool MmsHelper::sendMessage(const QStringList &to, const QStringList &cc, const QStringList &bcc, const QString &subject, const QVariantList &parts)
{
    return sendMessage(QString(), to, cc, bcc, subject, parts);
}

bool MmsHelper::sendMessage(const QString &imsi, const QStringList &to, const QStringList &cc,
    const QStringList &bcc, const QString &subject, const QVariantList &parts)
{
    QDir dir;
    dir.mkpath(TempDir::basePath());
    TempDir *tempDir = new TempDir;
    QDBusPendingCallWatcher *call = sendMessage(*tempDir, imsi, to, cc, bcc, subject, parts);
    if (call) {
        // TempDir object will keep the temporary directory around until
        // the call completes. Then everything gets deleted.
        connect(call, SIGNAL(finished(QDBusPendingCallWatcher*)), call, SLOT(deleteLater()));
        tempDir->setParent(call);
        return true;
    } else {
        // No call was submitted, delete the temporary stuff right away
        delete tempDir;
        return false;
    }
}

/* Parts should be an array of objects, each with the following:
 *   - contentId = string/integer uniquely representing the part within this message)
 *   - contentType = mime type, include charset for text
 *   - path = path to a file, which will be copied for the event
 *   - freeText = Instead of path, string contents for a textual part
 */
QDBusPendingCallWatcher *MmsHelper::sendMessage(const TempDir &tempDir,
    const QString &imsi, const QStringList &to, const QStringList &cc,
    const QStringList &bcc, const QString &subject, const QVariantList &parts)
{
    MmsPartList outParts;
    QMimeDatabase mimeDb;

    if (!tempDir.isValid()) {
        qWarning() << "MmsHelper::sendMessage: failed to create temporary dir";
        return NULL;
    }

    foreach (const QVariant &v, parts) {
        QVariantMap p = v.toMap();
        MmsPart part;
        part.contentId = p["contentId"].toString();
        part.contentType = p["contentType"].toString();
        if (part.contentId.isEmpty()) {
            qWarning() << "MmsHelper::sendMessage: missing contentId";
            return NULL;
        }

        part.fileName = p["path"].toString();
        if (part.fileName.isEmpty()) {
            QString freeText = p["freeText"].toString();
            if (!freeText.isEmpty()) {
                part.fileName = createTemporaryTextFile(tempDir.path(), mimeDb, freeText, part.contentId, part.contentType);
            }
            if (part.fileName.isEmpty()) {
                qWarning() << "MmsHelper::sendMessage: can't create temporary file";
                return NULL;
            }
        } else if (part.fileName.startsWith("file://")) {
            part.fileName = QUrl(part.fileName).toLocalFile();
        }

        if (part.contentType.isEmpty()) {
            QMimeType type = mimeDb.mimeTypeForFile(part.fileName);
            if (!type.isValid()) {
                qWarning() << "MmsHelper::sendMessage: Can't determine MIME type for file" << part.fileName;
                return NULL;
            }
            part.contentType = type.name();
        }

        outParts.append(part);
    }

    QVariantList params;
    if (!imsi.isNull())
        params << imsi;

    params << to << cc << bcc << subject << QVariant::fromValue(outParts);

    QDBusMessage call(QDBusMessage::createMethodCall(MMS_HANDLER_SERVICE,
        MMS_HANDLER_PATH, MMS_HANDLER_INTERFACE, "sendMessage"));
    call.setArguments(params);
    return new QDBusPendingCallWatcher(MMS_HANDLER_BUS.asyncCall(call));
}

bool MmsHelper::retrySendMessage(int eventId)
{
    QDBusMessage call(QDBusMessage::createMethodCall(MMS_HANDLER_SERVICE,
        MMS_HANDLER_PATH, MMS_HANDLER_INTERFACE, "sendMessageFromEvent"));
    call.setArguments(QVariantList() << eventId);
    MMS_HANDLER_BUS.asyncCall(call);
    return true;
}

#include "mmshelper.moc"
