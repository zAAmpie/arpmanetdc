#include "transfersegment.h"

TransferSegment::TransferSegment(QObject *parent) :
    QObject(parent)
{
    pDownloadBucketTable = 0;
}

TransferSegment::~TransferSegment(){}
void TransferSegment::transferTimerEvent(){}
void TransferSegment::setLastBucketNumber(int){}
void TransferSegment::setLastBucketSize(int){}
void TransferSegment::setFileSize(quint64){}
void TransferSegment::receivedPeerProtocolCapability(char){}

void TransferSegment::setSegmentStart(quint64 start)
{
    segmentStart = start;
    if (segmentEnd - segmentStart > 0)
        segmentLength = segmentEnd - segmentStart;
    calculateLastBucketParams();
}

void TransferSegment::setSegmentEnd(quint64 end)
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
    lastBucketNumber = segmentEnd >> 20;
    lastBucketSize = segmentEnd % HASH_BUCKET_SIZE;
}

quint64 TransferSegment::getSegmentStart()
{
    return segmentStart;
}

quint64 TransferSegment::getSegmentEnd()
{
    return segmentEnd;
}

qint64 TransferSegment::getSegmentStartTime()
{
    return segmentStartTime;
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
