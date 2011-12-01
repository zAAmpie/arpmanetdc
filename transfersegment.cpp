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
void TransferSegment::setFileSize(qint64){}

void TransferSegment::setFileOffset(quint64 offset)
{
    fileOffset = offset;
}

void TransferSegment::setFileOffsetLength(quint64 length)
{
    segmentLength = length;
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
