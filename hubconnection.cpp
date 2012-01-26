#include "hubconnection.h"

HubConnection::HubConnection(QObject *parent) :
    QObject(parent)
{
    hubSocket = new QTcpSocket(this);
    connect(hubSocket, SIGNAL(readyRead()), this, SLOT(processHubMessage()));
	connect(hubSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(socketError(QAbstractSocket::SocketError)));

	//Initialize variables - if they're not going to be set
	hubAddress = "127.0.0.1";
	hubPort = 4012;
	nick = "anon";
	password = "pass";
	version = "0";
	hubIsOnline = false;

	reconnectTimer = new QTimer(this);
	reconnectTimer->setInterval(30000); //Try to reconnect every 30 seconds
	connect(reconnectTimer, SIGNAL(timeout()), this, SLOT(reconnectTimeout()));

    keepaliveTimer = new QTimer(this);
    keepaliveTimer->setInterval(600000);  // every 10 minutes
    keepaliveTimer->setSingleShot(false);
    connect(keepaliveTimer, SIGNAL(timeout()), this, SLOT(keepaliveTimeout()));
}

HubConnection::HubConnection(QString address, quint16 port, QString nick, QString password, QString version, QObject *parent) : QObject(parent)
{
	hubSocket = new QTcpSocket(this);
    connect(hubSocket, SIGNAL(readyRead()), this, SLOT(processHubMessage()));
	connect(hubSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(socketError(QAbstractSocket::SocketError)));

	//Initialize variables - use set functions for type checking
	setHubAddress(address);
	setHubPort(port);
	setNick(nick);
	setPassword(password);
	setVersion(version);
	hubIsOnline = false;

	reconnectTimer = new QTimer(this);
	reconnectTimer->setInterval(30000); //Try to reconnect every 30 seconds
	connect(reconnectTimer, SIGNAL(timeout()), this, SLOT(reconnectTimeout()));
}

HubConnection::~HubConnection()
{
	//Destructor
	reconnectTimer->deleteLater();
	hubSocket->close();
	hubSocket->deleteLater();
}

void HubConnection::setHubAddress(QString addr)
{
    hubAddress = addr;
}

void HubConnection::setHubPort(quint16 port)
{
    hubPort = port;
}

void HubConnection::setNick(QString n)
{
    // TODO: check for invalid nick chars
    nick = n;
}

void HubConnection::setPassword(QString p)
{
    // TOOD: validity checks
    password = p;
}

void HubConnection::setVersion(QString v)
{
	version = v;
}

void HubConnection::sendChatMessage(QString message)
{
    QByteArray data;
    data.append("<");
    data.append(nick);
    data.append("> ");
    data.append(escapeDCProtocol(message));
    data.append("|");
    hubSocket->write(data);
}

void HubConnection::sendPrivateMessage(QString otherNick, QString message)
{
    QByteArray data;
    data.append("$To: ");
    data.append(otherNick);
    data.append(" From: ");
    data.append(nick);
    data.append(" $<");
    data.append(nick);
    data.append("> ");
    data.append(escapeDCProtocol(message));
    data.append("|");
    hubSocket->write(data);
}

QString HubConnection::escapeDCProtocol(QString msg)
{
    msg.replace(QString("$"), QString("&#36;"));
    msg.replace(QString("|"), QString("&#124;"));
    return msg;
}

QString HubConnection::unescapeDCProtocol(QString msg)
{
    msg.replace(QString("&#36;"), QString("$"));
    msg.replace(QString("&#124;"), QString("|"));
    return msg;
}

QString HubConnection::escapeHTMLTags(QString msg)
{
    msg.replace("<", "&lt;");
    msg.replace(">", "&gt;");
    msg.replace('"', "&quot;");
    return msg;
}

QString HubConnection::generateMyINFOString()
{
    QString s = "$MyINFO $ALL ";
    s.append(nick);
    s.append(" <ArpmanetDC V:");
    s.append(version);
    s.append(",M:A,H:");
    if (registeredUser)
        s.append("0/1/0");
    else
        s.append("1/0/0");
    s.append(",S:5>$ $100.00 KiB/s$$0$|");
    return s;
}

void HubConnection::processHubMessage()
{
	//Hub is considered online if it receives messages - for now
	if (!hubIsOnline)
	{
		hubIsOnline = true;
		if (reconnectTimer->isActive())
			reconnectTimer->stop();
		emit hubOnline();
	}

    while (hubSocket->bytesAvailable() > 0)
    {
        dataReceived.append(hubSocket->readAll());

		//Check if a full command was received and flag if not
        bool lastMessageHalf = false;
        if (QString(dataReceived.right(1)).compare("|") != 0)
            lastMessageHalf = true;

        QStringList msgs = QString(dataReceived).split("|");
        dataReceived.clear();

        QStringListIterator it(msgs);
        while (it.hasNext())
        {
            QString msg = it.next();
			//If a full command wasn't received, leave the rest of the data in the cache for the next read
            if (lastMessageHalf && !it.hasNext())
            {
                dataReceived.append(msg);
                continue;
            }

            if (msg.length() == 0)
                continue;

			//If command
            if (msg.mid(0, 1).compare("$") == 0)
            {
                QByteArray sendData;

				//===== MY INFO COMMAND =====
                if (msg.mid(0, 7).compare("$MyINFO") == 0)
                {
                    // $MyINFO $ALL nick description<ArpmanetDC V:0.1,M:A, etc
                    int pos1 = msg.indexOf(" ", 13);
                    QString nickname = msg.mid(13, pos1 - 13);
                    int pos2 = msg.indexOf("<", pos1 + 1);
                    QString description = msg.mid(pos1 + 1, pos2 - pos1 - 1);

					pos1 = msg.indexOf("V:", pos2 + 1);
					QString client = msg.mid(pos2 + 1, pos1 - pos2 - 2);
                    
                    QString version = msg.mid(pos1 + 2, msg.indexOf("M:", pos2 + 1) - pos1 - 3);

                    pos1 = msg.indexOf("M:", pos2 + 1);
                    QString mode = msg.mid(pos1 + 2, 1);
                    emit receivedMyINFO(nickname, description, mode, client, version);
                }

				//===== USER LEFT =====
                else if (msg.mid(0, 5).compare("$Quit") == 0)
                {
                    emit userLoggedOut(msg.mid(6));
                }

				//===== AUTHENTICATE CAPABILITIES =====
                else if (msg.mid(0, 5).compare("$Lock") == 0)
                {
                    sendData.append("$Supports NoGetINFO NoHello TTHSearch |$Key ABCABCABC|$ValidateNick ");
                    sendData.append(nick);
                    sendData.append("|");
                    hubSocket->write(sendData);
                }

				//===== AUTHENTICATE USER =====
                else if (msg.mid(0, 6).compare("$Hello") == 0)
                {
                    if (msg.mid(7).compare(nick) == 0)
                    {
                        sendData.append("$Version 1,0091|$GetNickList|");
                        sendData.append(generateMyINFOString());
                        hubSocket->write(sendData);
                    }
                    else
                    {
                        emit(userLoggedIn(msg.mid(7)));
                    }
                }

				//===== AUTHENTICATE PASSWORD =====
                else if (msg.mid(0, 8).compare("$GetPass") == 0)
                {
                    sendData.append("$MyPass ");
                    sendData.append(password);
                    sendData.append("|");
                    hubSocket->write(sendData);
                    registeredUser = true;
                }

				//===== PRIVATE MESSAGE =====
                else if (msg.mid(0, 4).compare("$To:") == 0)
                {
                    QString str = tr("%1 From: ").arg(nick);
                    int pos1 = msg.indexOf(str);
                    int pos2 = msg.indexOf(" ", pos1 + str.size());
                    QString otherNick = msg.mid(pos1 + str.size(), pos2 - pos1 - str.size());
                    QString message = msg.mid(pos2 + 2);
                    emit receivedPrivateMessage(otherNick, escapeHTMLTags(escapeDCProtocol(message)));
                }

				//===== NICKNAME LIST =====
                else if (msg.mid(0,9).compare("$NickList") == 0)
                {
                    msg = msg.mid(msg.indexOf("$$")+2);
                    QStringList nicks = msg.split("$$");
                    emit receivedNickList(nicks);
                }

				//===== OP LIST =====
                else if (msg.mid(0,7).compare("$OpList") == 0)
                {
                    msg = msg.mid(msg.indexOf("$$")+2);
                    QStringList nicks = msg.split("$$");
                    emit receivedOpList(nicks);
                }
            }
            else
            {
				//===== MAIN CHAT MESSAGE =====
                lastChatMessage = escapeHTMLTags(unescapeDCProtocol(msg));
                emit receivedChatMessage(lastChatMessage);
            }
        }
    }
}

void HubConnection::connectHub()
{
	//Make sure there is something to connect to
	if (!hubAddress.isEmpty() && hubPort != 0)
	{
		//Check if not already connecting - in that case just wait
		if (hubSocket->state() != QAbstractSocket::ConnectingState)
		{
			registeredUser = false;
            hubSocket->close();
			hubSocket->connectToHost(hubAddress, hubPort);
            hubSocket->setReadBufferSize(10*(1<<20)); //Set TCP read buffer to 10MB

			emit receivedChatMessage(tr("&lt;::info&gt;Connecting to %1:%2...").arg(hubAddress).arg(hubPort));
		}
	}
	else
	{
		//If address/port is not valid, stop reconnection process cause it's never going to work
		if (hubIsOnline)
		{
			hubIsOnline = false;
			emit hubOffline();
		}
		if (reconnectTimer->isActive())
			reconnectTimer->stop();
	}
}

void HubConnection::socketError(QAbstractSocket::SocketError error)
{
	//Parse error
	QString errorString;

	switch(error)
	{
		case QAbstractSocket::ConnectionRefusedError:
			errorString = "Connection refused by server.";
			break;

		case QAbstractSocket::RemoteHostClosedError:
			errorString = "Remote host forcibly closed the connection.";
			break;

		case QAbstractSocket::NetworkError:
			errorString = "Network error.";
			break;

		case QAbstractSocket::HostNotFoundError:
			errorString = "Remote host could not be found.";
			break;

		default:
			errorString = tr("Error occurred: %1").arg(hubSocket->errorString());
			break;
	}
		
	//Only try reconnecting if there was a connection error - otherwise just show there was an error
	if (hubSocket->state() == QAbstractSocket::UnconnectedState || hubSocket->state() == QAbstractSocket::ClosingState || hubSocket->state() == QAbstractSocket::ConnectedState)
	{
		//Try again if it was a connection error
		if (!reconnectTimer->isActive())
        {
            //Check if the error was due to bad nickname
            int interval = 30000; //30 seconds
            if (lastChatMessage.contains("&lt;-VerliHub-&gt; Bad nickname: Wait"))
            {
                int pos1 = lastChatMessage.indexOf("Wait") + 4;
                int pos2 = lastChatMessage.indexOf("sec");

                interval = lastChatMessage.mid(pos1, pos2 - pos1).toInt() * 1000;
            }

            errorString += tr(" Retrying in %1 second%2...").arg(interval / 1000).arg((interval / 1000) != 1 ? "s" : "");
			reconnectTimer->start(interval);
        }
	}	

    //If hub was online, change status to offline
    if (hubIsOnline)
    {
        hubIsOnline = false;
        emit hubOffline();
    }

	emit hubError(errorString);
    emit receivedChatMessage("&lt;::error&gt;" + errorString);
}

void HubConnection::reconnectTimeout()
{
	//Retry connection
	connectHub();
}

void HubConnection::keepaliveTimeout()
{
    if (hubIsOnline)
        hubSocket->write(generateMyINFOString().toAscii());
}

// Get functions, so that we do not need to reconnect on every settings change
QString HubConnection::getHubAddress()
{
    return hubAddress;
}

quint16 HubConnection::getHubPort()
{
    return hubPort;
}

QString HubConnection::getNick()
{
    return nick;
}

QString HubConnection::getPassword()
{
    return password;
}
