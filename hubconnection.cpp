#include "hubconnection.h"

HubConnection::HubConnection(QObject *parent) :
    QObject(parent)
{
    hubSocket = new QTcpSocket(this);
    connect(hubSocket, SIGNAL(readyRead()), this, SLOT(processHubMessage()));
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

void HubConnection::processHubMessage()
{
    while (hubSocket->bytesAvailable() > 0)
    {
        dataReceived.append(hubSocket->readAll());
        bool lastMessageHalf = false;
        if (QString(dataReceived.right(1)).compare("|") != 0)
            lastMessageHalf = true;

        QStringList msgs = QString(dataReceived).split("|");
        dataReceived.clear();

        QStringListIterator it(msgs);
        while (it.hasNext())
        {
            QString msg = it.next();
            if (lastMessageHalf && !it.hasNext())
            {
                dataReceived.append(msg);
                continue;
            }
            if (msg.length() == 0)
                continue;

            if (msg.mid(0, 1).compare("$") == 0)
            {
                QByteArray sendData;
                if (msg.mid(0, 7).compare("$MyINFO") == 0)
                {
                    // $MyINFO $ALL nick description<ArpmanetDC V:0.1,M:A, etc
                    int pos1 = msg.indexOf(" ", 13);
                    QString nickname = msg.mid(13, pos1 - 13);
                    int pos2 = msg.indexOf("<", pos1 + 1);
                    QString description = msg.mid(pos1 + 1, pos2 - pos1 - 1);
                    pos1 = msg.indexOf("M:", pos2 + 1);
                    QString mode = msg.mid(pos1 + 2, 1);
                    emit receivedMyINFO(nickname, description, mode);
                }
                else if (msg.mid(0, 5).compare("$Quit") == 0)
                {
                    emit userLoggedOut(msg.mid(6));
                }
                else if (msg.mid(0, 5).compare("$Lock") == 0)
                {
                    sendData.append("$Supports NoGetINFO NoHello TTHSearch |$Key ABCABCABC|$ValidateNick ");
                    sendData.append(nick);
                    sendData.append("|");
                    hubSocket->write(sendData);
                }
                else if (msg.mid(0, 6).compare("$Hello") == 0)
                {
                    if (msg.mid(7).compare(nick) == 0)
                    {
                        sendData.append("$Version 1,0091|$GetNickList|$MyINFO $ALL ");
                        sendData.append(nick);
						sendData.append(" [L:2.00 MiB/s]<ArpmanetDC V:0.1,M:A,H:");
                        if (registeredUser)
                            sendData.append("0/1/0");
                        else
                            sendData.append("1/0/0");
                        sendData.append(",S:5>$ $100.00 KiB/s$$0$|");
                        hubSocket->write(sendData);
                    }
                    else
                    {
                        emit(userLoggedIn(msg.mid(7)));
                    }
                }
                else if (msg.mid(0, 8).compare("$GetPass") == 0)
                {
                    sendData.append("$MyPass ");
                    sendData.append(password);
                    sendData.append("|");
                    hubSocket->write(sendData);
                    registeredUser = true;
                }
                else if (msg.mid(0, 4).compare("$To:") == 0)
                {
					QString str = tr("%1 From: ").arg(nick);
                    int pos1 = msg.indexOf(str);
                    int pos2 = msg.indexOf(" ", pos1 + str.size());
                    QString otherNick = msg.mid(pos1 + str.size(), pos2 - pos1 - str.size());
                    QString message = msg.mid(pos2 + 2);
                    emit receivedPrivateMessage(otherNick, escapeDCProtocol(message));  // TODO: nie seker of indekse en offsets reg is nie, check!
                }
				else if (msg.mid(0,9).compare("$NickList") == 0)
				{
					msg = msg.mid(msg.indexOf("$$")+2);
					QStringList nicks = msg.split("$$");
					emit receivedNickList(nicks);
				}
            }
            else
                emit receivedChatMessage(unescapeDCProtocol(msg));

        }
    }
}

void HubConnection::connectHub()
{
    registeredUser = false;
    hubSocket->connectToHost(hubAddress, hubPort);
}
