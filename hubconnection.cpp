#include "hubconnection.h"

#ifdef Q_WS_WIN //If windows
#include <winsock2.h>
#include <ws2tcpip.h>
#else //If Q_WS_X11
#include <sys/socket.h>
#include <sys/types.h>
#endif

HubConnection::HubConnection(QObject *parent) :
QObject(parent)
{
    hubSocket = new QTcpSocket(this);
    connect(hubSocket, SIGNAL(readyRead()), this, SLOT(processHubMessage()));
    connect(hubSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(socketError(QAbstractSocket::SocketError)));
    connect(hubSocket, SIGNAL(connected()), this, SLOT(socketConnected()));

    //Initialize variables - if they're not going to be set
    hubAddress = "127.0.0.1";
    hubPort = 4012;
    nick = "anon";
    password = "pass";
    version = "0";
    hubIsOnline = false;

    totalShareSize = 0;

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
    connect(hubSocket, SIGNAL(connected()), this, SLOT(socketConnected()));

    //Initialize variables - use set functions for type checking
    setHubAddress(address);
    setHubPort(port);
    setNick(nick);
    setPassword(password);
    setVersion(version);
    hubIsOnline = false;

    totalShareSize = 0;

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

void HubConnection::setTotalShareSize(quint64 size)
{
    totalShareSize = size;
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
    s.append(tr(",S:5>$ $100.00 KiB/s$$%1$|").arg(totalShareSize));
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
                    //Different types of MyINFOs that the RegExp has to match
                    // $MyINFO $ALL nick description<ArpmanetDC V:0.1,M:A, etc
                    // $MyINFO $ALL Szalor35 <ArpmanetDC V:0.1.5,M:A,H:0/1/0,S:5>$ $100.00 KiB/s$$0$
                    // $MyINFO $ALL Tumi <ApexDC++ V:1.3.9,M:A,H:1/0/0,S:20>$ $100$$0$
                    // $MyINFO $ALL 94171-376 $ $0.005$$0$
                    // $MyINFO $ALL -Trivia- As twak praat 'n bossie was.....$ $Hub$$0$
                    // $MyINFO $ALL -OpChat- Operator chat - only for OPs$ $$$0$
                    QString regex = "\\$MyINFO \\$ALL ([^ ]*) ([^\\$]*)\\$ \\$([^\\$]*)\\$\\$([0-9]+)\\$";
                    QRegExp rx(regex, Qt::CaseInsensitive);

                    //Check for regex's
                    if (rx.indexIn(msg) != -1)
                    {
                        //Extract normal information
                        QString nick = rx.cap(1);
                        QString desc = rx.cap(2);
                        QString extra = rx.cap(3);
                        quint64 shareBytes = rx.cap(4).toULongLong();

                        //If a client information is embedded in the description, parse it
                        if (!desc.isEmpty() && desc.contains("<"))
                        {
                            //Typical format of a client description
                            //<ArpmanetDC V:0.1.5,M:A,H:0/1/0,S:5>
                            QString clientRegex = "([^<]*)<(.*) V:([0-9\\.]*),M:([a-z0-9]{1}),H:([0-9\\/]*),S:([0-9]*)>";
                            QRegExp rxc(clientRegex, Qt::CaseInsensitive);

                            if (rxc.indexIn(desc) != -1)
                            {
                                //Extract additional client information
                                QString description = rxc.cap(1); //The actual description without client info attached
                                QString client = rxc.cap(2);
                                QString version = rxc.cap(3);
                                QString mode = rxc.cap(4);
                                QString registerMode = rxc.cap(5); //Not used ATM, but can be implemented later
                                quint16 openSlots = rxc.cap(6).toUInt(); //Not really used anymore

                                //Client information parsed correctly, fill in all fields
                                emit receivedMyINFO(nick, description, mode, client, version, registerMode, openSlots, shareBytes);
                            }
                        }
                        else
                        {
                            //No client information is given, just use normal description and leave the rest of the fields blank
                            emit receivedMyINFO(nick, desc, ""/*mode*/, extra/*client*/, ""/*version*/, ""/*registerMode*/, 0/*openSlots*/, shareBytes);
                        }

                    }
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
                    msg = msg.mid(msg.indexOf("$$")+2); //Remove first $$
                    msg = msg.left(msg.lastIndexOf("$$")); //Remove last $$
                    QStringList nicks = msg.split("$$"); //Split to get all ops
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

//Slot called when hub socket is connected with hub
void HubConnection::socketConnected()
{
    //Set TCP send buffer to 4MB
    int size = 4 * 1<<20;
    if (::setsockopt(hubSocket->socketDescriptor(), SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size)) == -1)
    {
        qDebug() << "HubConnection::socketConnected: Could not set send buffer to 4MB";
    }
    if (::setsockopt(hubSocket->socketDescriptor(), SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size)) == -1)
    {
        qDebug() << "HubConnection::socketConnected: Could not set receive buffer to 4MB";
    }

    //Try to keep the socket alive
    hubSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    hubSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    //Set LINGER Option
    linger pLinger;
    pLinger.l_onoff = 0; //Disable socket lingering to avoid keeping the socket open when trying to send data as the socket closes - can't really hurt to disable it
    pLinger.l_linger = 0;

    if (::setsockopt(hubSocket->socketDescriptor(), SOL_SOCKET, SO_LINGER, (char *)&pLinger, sizeof(pLinger)) == -1)
    {
        qDebug() << "HubConnection::socketConnected: Could not disable socket lingering";
    }
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
