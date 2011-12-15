#ifndef UTPSOCKET_H
#define UTPSOCKET_H

#include <QObject>
#include "libutp/utp.h"

class UtpSocket : public QObject
{
    Q_OBJECT

public:
    UtpSocket(QObject *parent = 0);
    ~UtpSocket();

private:
    UTPSocket *pSocket;
};

#endif