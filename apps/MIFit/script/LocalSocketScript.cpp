#include "LocalSocketScript.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <ui/Logger.h>
#include "MIFitScriptObject.h"

namespace
{
const quint32 kMaxPayload = 16 * 1024 * 1024;
}

LocalSocketScript::LocalSocketScript(QObject *parent)
    : QObject(parent),
      commands(new MIFitScriptObject)
{
    const QString serverName = "MIFit-" + QString::number(static_cast<quint64>(QCoreApplication::applicationPid()), 16);

    localServer = new QLocalServer(this);
    localServer->setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(serverName); // stale socket from a crashed session
    if (!localServer->listen(serverName))
    {
        Logger::log("Local scripting socket failed: %s", localServer->errorString().toLocal8Bit().constData());
    }
    connect(localServer, SIGNAL(newConnection()),
            this, SLOT(handleConnection()));
}

LocalSocketScript::~LocalSocketScript()
{
    delete commands;
}

QString LocalSocketScript::name() const
{
    return localServer->fullServerName();
}

void LocalSocketScript::handleConnection()
{
    while (QLocalSocket *connection = localServer->nextPendingConnection())
    {
        buffers.insert(connection, QByteArray());
        connect(connection, SIGNAL(readyRead()), this, SLOT(handleReadyRead()));
        connect(connection, SIGNAL(disconnected()), this, SLOT(handleDisconnected()));
        if (connection->bytesAvailable() > 0)
        {
            readFrom(connection); // data may have arrived before readyRead was connected
        }
    }
}

void LocalSocketScript::handleDisconnected()
{
    QLocalSocket *connection = qobject_cast<QLocalSocket*>(sender());
    if (connection)
    {
        buffers.remove(connection);
        connection->deleteLater();
    }
}

void LocalSocketScript::handleReadyRead()
{
    readFrom(qobject_cast<QLocalSocket*>(sender()));
}

void LocalSocketScript::readFrom(QLocalSocket *connection)
{
    if (!connection || !buffers.contains(connection))
    {
        return;
    }
    buffers[connection].append(connection->readAll());
    processBuffer(connection);
}

void LocalSocketScript::processBuffer(QLocalSocket *connection)
{
    QByteArray &buf = buffers[connection];
    while (buf.size() >= 4)
    {
        const uchar *p = reinterpret_cast<const uchar*>(buf.constData());
        const quint32 length = (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]);
        if (length > kMaxPayload)
        {
            Logger::log("Script request too large (%u bytes); closing connection", length);
            connection->disconnectFromServer();
            return;
        }
        if (static_cast<quint32>(buf.size()) < 4 + length)
        {
            return; // wait for the rest
        }
        const QByteArray payload = buf.mid(4, length);
        buf.remove(0, 4 + length);

        QJsonObject reply;
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        {
            reply["ok"] = false;
            reply["error"] = "invalid JSON request";
        }
        else
        {
            QJsonObject request = doc.object();
            reply = commands->execute(request["cmd"].toString(), request["args"].toArray());
        }
        sendReply(connection, QJsonDocument(reply).toJson(QJsonDocument::Compact));
    }
}

void LocalSocketScript::sendReply(QLocalSocket *connection, const QByteArray &json)
{
    const quint32 length = static_cast<quint32>(json.size());
    QByteArray frame;
    frame.append(char((length >> 24) & 0xff));
    frame.append(char((length >> 16) & 0xff));
    frame.append(char((length >> 8) & 0xff));
    frame.append(char(length & 0xff));
    frame.append(json);
    connection->write(frame);
}
