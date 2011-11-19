// TODO's:
// Check of die parsing van die inkomende PM reg gebeur
// Check of die hub se connection suksesvol deurgaan en hou
// TImer reconnect as die hub connection af is

#ifndef HUBCONNECTION_H
#define HUBCONNECTION_H

#include <QObject>
#include <QtNetwork/QTcpSocket>
#include <QStringList>

class HubConnection : public QObject
{
    Q_OBJECT
public:
    explicit HubConnection(QObject *parent = 0);
    void connectHub();
    void setHubAddress(QString);
    void setHubPort(quint16);
    void setNick(QString);
    void setPassword(QString);
    void sendPrivateMessage(QString, QString);
    void sendChatMessage(QString);

signals:
    void receivedChatMessage(QString);
    void receivedPrivateMessage(QString, QString);
    void userLoggedIn(QString);
    void userLoggedOut(QString);
    void receivedNickList(QStringList);
    void receivedOpList(QStringList);
    void receivedMyINFO(QString, QString, QString); // nick, description, mode

private slots:
    void processHubMessage();

private:
    QTcpSocket *hubSocket;
    QString hubAddress;
    quint16 hubPort;
    QString nick;
    QString password;
    bool registeredUser;
    QByteArray dataReceived;
    QString escapeDCProtocol(QString);
    QString unescapeDCProtocol(QString);

};

#endif // HUBCONNECTION_H
