/* This file is part of ArpmanetDC. Copyright (C) 2012
 * Source code can be found at http://code.google.com/p/arpmanetdc/
 * 
 * ArpmanetDC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ArpmanetDC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ArpmanetDC.  If not, see <http://www.gnu.org/licenses/>.
 */

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
    QString getHubAddress();
    quint16 getHubPort();
    QString getNick();
    QString getPassword();

signals:
    void receivedChatMessage(QString message);
    void receivedPrivateMessage(QString otherNick, QString message);
    void userLoggedIn(QString nick);
    void userLoggedOut(QString nick);
    void receivedNickList(QStringList nickList);
    void receivedOpList(QStringList opList);
    void receivedMyINFO(QString nick, QString description, QString mode, QString client, QString version);

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

    //Keepalive timer expired
    void keepaliveTimeout();

private:
	//Escaping functions
    QString escapeDCProtocol(QString);
    QString unescapeDCProtocol(QString);
    QString generateMyINFOString();

	//Objects
	QTcpSocket *hubSocket;
	QTimer *reconnectTimer;
    QTimer *keepaliveTimer;

	//Parameters
    QString lastChatMessage;
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
