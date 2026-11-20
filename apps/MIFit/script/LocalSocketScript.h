#ifndef script_LocalSocketScript_h
#define script_LocalSocketScript_h

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
class QLocalServer;
class QLocalSocket;
class MIFitScriptObject;

/**
 * Local-socket server for external scripts (MIExpert).
 * Framing (both directions): 4-byte big-endian payload length, then UTF-8 JSON.
 * Request: {"cmd": "<name>", "args": [...]}
 * Reply:   {"ok": true, "result": ...} or {"ok": false, "error": "<message>"}
 */
class LocalSocketScript : public QObject
{
    Q_OBJECT

public:
    LocalSocketScript(QObject *parent = 0);
    ~LocalSocketScript();

    // Full server name; on Linux this is the Unix domain socket path
    QString name() const;

private:
    void readFrom(QLocalSocket *connection);
    void processBuffer(QLocalSocket *connection);
    void sendReply(QLocalSocket *connection, const QByteArray &json);

    QLocalServer *localServer;
    MIFitScriptObject *commands;
    QHash<QLocalSocket*, QByteArray> buffers;

private slots:
    void handleConnection();
    void handleReadyRead();
    void handleDisconnected();
};

#endif // ifndef script_LocalSocketScript_h
