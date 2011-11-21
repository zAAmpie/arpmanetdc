#include "transfer.h"

Transfer::Transfer(QObject *parent) :
    QObject(parent)
{
}

Transfer::~Transfer()
{
}

// empty base class definitions, since these do not make sense for uploads
void Transfer::incomingDataPacket(quint8, quint64&, QByteArray&)
{}
void Transfer::hashBucketReply(int&, QByteArray&)
{}
void Transfer::TTHTreeReply(QByteArray&, QByteArray&)
{}
// ------------------------------------------------------------------------

void Transfer::setFileName(QString &filename)
{
    filePathName = filename;
}

void Transfer::setTTH(QByteArray &tth)
{
    TTH = tth;
}

void Transfer::setFileOffset(quint64 offset)
{
    fileOffset = offset;
}

void Transfer::setSegmentLength(quint64 length)
{
    segmentLength = length;
}

void Transfer::setRemoteHost(QHostAddress remote)
{
    remoteHost = remote;
}

void Transfer::setTransferProtocol(quint8 protocol)
{
    transferProtocol = protocol;
}

QByteArray* Transfer::getTTH()
{
    return &TTH;
}

QString* Transfer::getFileName()
{
    return &filePathName;
}

QHostAddress* Transfer::getRemoteHost()
{
    return &remoteHost;
}

quint64* Transfer::getTransferRate()
{
    return &transferRate;
}

int* Transfer::getTransferStatus()
{
    return &status;
}

int* Transfer::getTransferProgress()
{
    return &transferProgress;
}

void Transfer::addPeer(QHostAddress &peer)
{
    if (!listOfPeers.contains(peer))
        listOfPeers.append(peer);
}
