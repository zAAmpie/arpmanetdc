#include "transfer.h"

Transfer::Transfer(QObject *parent) : QObject(parent)
{
    upTime = QDateTime::currentDateTime();
    fileExists = false;
}

Transfer::~Transfer()
{
}

// empty base class definitions, since these do not make sense for uploads
void Transfer::incomingDataPacket(quint8, qint64, QByteArray){}
void Transfer::hashBucketReply(int, QByteArray, QHostAddress){}
void Transfer::TTHTreeReply(QByteArray){}
void Transfer::receivedPeerProtocolCapability(QHostAddress, quint8){}
TransferSegment* Transfer::createUploadObject(quint8, quint32){return 0;}
void Transfer::bucketFlushed(int){}
void Transfer::bucketFlushFailed(int){}
void Transfer::incomingTransferError(quint64, quint8){}
void Transfer::setNextSegmentId(quint32){}
void Transfer::addPeer(QHostAddress,QByteArray){}
void Transfer::setBucketFlushStateBitmap(QByteArray){}
int Transfer::getSegmentCount() {return 0;}
SegmentStatusStruct Transfer::getSegmentStatuses() {SegmentStatusStruct s = {0,0,0,0,0}; return s;}

// ------------------------------------------------------------------------

void Transfer::setFileName(QString filename)
{
    filePathName = filename;
    fileExists = false;
    if (QFile::exists(filePathName + ".incomplete"))
        fileExists = true;
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

void Transfer::setFileOffset(qint64 offset)
{
    fileOffset = offset;
}

void Transfer::setSegmentLength(qint64 length)
{
    segmentLength = length;
}

void Transfer::setRemoteHost(QHostAddress remote)
{
    remoteHost = remote;
}

/*void Transfer::setTransferProtocolHint(QByteArray &protocolHint)
{
    transferProtocolHint = protocolHint;
}*/

void Transfer::setFileSize(quint64 size)
{
    fileSize = size;
}

void Transfer::setProtocolOrderPreference(QByteArray p)
{
    protocolOrderPreference = p;
}

void Transfer::setCurrentlyDownloadingPeers(QSet<QHostAddress> *dh)
{
    pCurrentDownloadingPeers = dh;
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

qint64 Transfer::getTransferRate()
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

QByteArray Transfer::getTransferStateBitmap()
{
    return QByteArray();
}

qint64 Transfer::getUptime()
{
    return QDateTime::currentMSecsSinceEpoch() - upTime.toMSecsSinceEpoch();
}
