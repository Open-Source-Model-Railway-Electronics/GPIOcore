#pragma once
// Phone remote: a tiny HTTP server in the GUI. A phone on the same network opens
// http://<pc>:<port>/?t=<token> (shown as a QR code) and can move servos live, store their
// curved / straight positions and switch them over. Everything goes through the main window,
// so the phone does exactly what the GUI would do.

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <functional>

class QTcpServer;
class QTcpSocket;

class WebRemote : public QObject
{
    Q_OBJECT
public:
    explicit WebRemote(QObject *parent = nullptr);

    bool    start(quint16 port = 8080);   // tries port .. port+9
    void    stop();
    bool    isRunning() const;
    quint16 port() const;
    QString token() const { return m_token; }
    QString url() const;                  // http://<LAN address>:<port>/?t=<token>, empty when stopped

    // JSON for /api/state: { board, connected, servos: [ {index, name, min, max, speed, state} ] }
    std::function<QJsonObject()> stateProvider;

signals:
    void liveRequested(int output, int angle);
    void storeRequested(int output, int which, int angle);   // which: 0 = curved, 1 = straight
    void moveRequested(int output, int state);

private:
    void onConnection();
    void handle(QTcpSocket *socket);
    void reply(QTcpSocket *socket, int status, const QByteArray &type, const QByteArray &body);

    QTcpServer *m_server = nullptr;
    QString     m_token;
};

QString lanAddress();                     // the PC's IPv4 address on the local network
