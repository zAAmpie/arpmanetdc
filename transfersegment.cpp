#include "transfersegment.h"

TransferSegment::TransferSegment(QObject *parent) :
    QObject(parent)
{
    pDownloadBucketTable = 0;
    maxUploadRequestOffset = 0;
    segmentStart = 0;
    segmentLength = 0;
    segmentEnd = 0;
    bytesTransferred = 0;
}

TransferSegment::~TransferSegment()
{
    emit removeTransferSegmentPointer(segmentId);
    //qDebug() << "TransferSegment DESTROYING: " << this;
}

void TransferSegment::transferTimerEvent(){}
void TransferSegment::setFileSize(quint64){}
//void TransferSegment::receivedPeerProtocolCapability(char){}
qint64 TransferSegment::getBytesReceivedNotFlushed(){return 0;}
qint64 TransferSegment::getMaxUploadRequestOffset(){return maxUploadRequestOffset;}

void TransferSegment::setSegmentStart(qint64 start)
{
    segmentStart = start;
    segmentLength = segmentEnd - segmentStart > 0 ? segmentEnd - segmentStart : 0;
    calculateLastBucketParams();
}

void TransferSegment::setSegmentEnd(qint64 end)
{
    segmentEnd = end;
    if (segmentEnd - segmentStart > 0)
    {
        segmentLength = segmentEnd - segmentStart;
    }
    else
    {
        segmentLength = 0;
        segmentEnd = segmentStart;
    }
    calculateLastBucketParams();
}

void TransferSegment::calculateLastBucketParams()
{
    lastBucketNumber = segmentEnd % HASH_BUCKET_SIZE == 0 ? (segmentEnd >> 20) - 1: segmentEnd >> 20;
    lastBucketNumber = lastBucketNumber < 0 ? 0 : lastBucketNumber;
    lastBucketSize = segmentLength % HASH_BUCKET_SIZE == 0 ? HASH_BUCKET_SIZE : segmentLength % HASH_BUCKET_SIZE;
}

qint64 TransferSegment::getSegmentStart()
{
    return segmentStart;
}

qint64 TransferSegment::getSegmentEnd()
{
    return segmentEnd;
}

qint64 TransferSegment::getSegmentStartTime()
{
    return segmentStartTime;
}

QHostAddress TransferSegment::getSegmentRemotePeer()
{
    return remoteHost;
}

int TransferSegment::getSegmentStatus()
{
    return status;
}

quint32 TransferSegment::getSegmentId()
{
    return segmentId;
}

quint64 TransferSegment::getBytesTransferred()
{
    return bytesTransferred;
}

void TransferSegment::setTTH(QByteArray tth)
{
    TTH = tth;
}

void TransferSegment::setRemoteHost(QHostAddress host)
{
    remoteHost = host;
}

void TransferSegment::setDownloadBucketTablePointer(QHash<int, QByteArray *> *dbt)
{
    pDownloadBucketTable = dbt;
}

void TransferSegment::setSegmentId(quint32 id)
{
    segmentId = id;
}

void TransferSegment::checkSendDownloadRequest(quint8 protocol, QHostAddress peer, QByteArray TTH,
                                                       qint64 requestingOffset, qint64 requestingLength, int status)
{
    if (status & (TRANSFER_STATE_RUNNING | TRANSFER_STATE_STALLED))
    {
        if (segmentEnd < requestingOffset + requestingLength)
            requestingLength = segmentEnd - requestingOffset;
        if (requestingLength > 0)
        {
            //qDebug() << "TransferSegment::checkSendDownloadRequest() emit sendDownloadRequest() peer tth offset length segmentid "
            //         << peer << TTH.toBase64() << requestingOffset << requestingLength << segmentId;
            emit sendDownloadRequest(protocol, peer, TTH, requestingOffset, requestingLength, segmentId);
        }
    }
    else
    {
        qDebug() << "TransferSegment::checkSendDownloadRequest drop download request: invalid state: " << status;
    }
}

int TransferSegment::calculateBucketNumber(quint64 fileOffset)
{
    return (int)(fileOffset >> 20);
}
