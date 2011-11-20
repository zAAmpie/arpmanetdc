#ifndef HUBCONNECTION_H
#define HUBCONNECTION_H

#include <QObject>
#include <QtNetwork/QTcpSocket>
#include <QStringList>
#include <QTimer>

class HubConnection : public QObject
{
    Q_OBJECT
public:
    explicit HubConnection(QObject *parent = 0);
	explicit HubConnection(QString address, quint16 port, QString nick, QString password, QString version, QObject *parent = 0);
	~HubConnection();

    void connectHub();
    void setHubAddress(QString address);
    void setHubPort(quint16 port);
    void setNick(QString nick);
    void setPassword(QString pass);
	void setVersion(QString version);
    void sendPrivateMessage(QString otherNick, QString message);
    void sendChatMessage(QString message);

signals:
    void receivedChatMessage(QString message);
    void receivedPrivateMessage(QString otherNick, QString message);
    void userLoggedIn(QString nick);
    void userLoggedOut(QString nick);
    void receivedNickList(QStringList nickList);
    void receivedOpList(QStringList opList);
    void receivedMyINFO(QString nick, QString description, QString mode);

	//Emitted once when hub goes offline
	void hubOffline();
	//Emitted once when hub goes online
	void hubOnline();
	//Socket error emitted every time
	void hubError(QString error);

private slots:
	//Received a message from the hub
    void processHubMessage();

	//Hub socket produced an error
	void socketError(QAbstractSocket::SocketError error);

	//Reconnect timer expired
	void reconnectTimeout();

private:
	//Escaping functions
    QString escapeDCProtocol(QString);
    QString unescapeDCProtocol(QString);

	//Objects
	QTcpSocket *hubSocket;
	QTimer *reconnectTimer;

	//Parameters
    QString hubAddress;
    quint16 hubPort;
    QString nick;
    QString password;
	QString version;
    bool registeredUser;
    QByteArray dataReceived;

	bool hubIsOnline;
};

#endif // HUBCONNECTION_H
