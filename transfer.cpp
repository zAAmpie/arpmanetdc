#include "transfer.h"

Transfer::Transfer(QObject *parent) :
    QObject(parent)
{
}

Transfer::~Transfer()
{
}

// empty base class definitions, since these do not make sense for uploads
void Transfer::incomingDataPacket(quint8, quint64, QByteArray){}
void Transfer::hashBucketReply(int, QByteArray){}
void Transfer::TTHTreeReply(QByteArray){}
void Transfer::transferTimerEvent(){}

// ------------------------------------------------------------------------

void Transfer::setFileName(QString filename)
{
    filePathName = filename;
}

void Transfer::setTTH(QByteArray tth)
{
    TTH = tth;
    TTHBase32 = tth;
    if (!base32Encode(TTHBase32))
    {
        // TODO: emit MISTAKE!
    }
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

void Transfer::setTransferProtocolHint(QByteArray &protocolHint)
{
    transferProtocolHint = protocolHint;
}

void Transfer::setFileSize(quint64 size)
{
    fileSize = size;
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

quint64 Transfer::getTransferRate()
{
    return transferRate;
}

quint64 Transfer::getFileSize()
{
    return fileSize;
}

int Transfer::getTransferStatus()
{
    return status;
}

int Transfer::getTransferProgress()
{
    return transferProgress;
}

void Transfer::addPeer(QHostAddress peer)
{
    if (peer.toIPv4Address() > 0 && !listOfPeers.contains(peer))
        listOfPeers.append(peer);
}
