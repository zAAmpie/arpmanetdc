#include "fstptransfersegment.h"

FSTPTransferSegment::FSTPTransferSegment()
{
    requestingOffset = 0;
    requestingLength = 65536;
    requestingTargetOffset = 0;
}

FSTPTransferSegment::~FSTPTransferSegment()
{
    inputFile.close();
}

void FSTPTransferSegment::setFileName(QString filename)
{
    filePathName = filename;
    inputFile.setFileName(filePathName);
    inputFile.open(QIODevice::ReadOnly);
    fileSize = inputFile.size();
}

void FSTPTransferSegment::setLastBucketNumber(int n)
{
    lastBucketNumber = n;
}

void FSTPTransferSegment::setLastBucketSize(int size)
{
    lastBucketSize = size;
}

void FSTPTransferSegment::startUploading()
{
    if (fileOffset > fileSize)
        return;
    else if (fileOffset + segmentLength > fileSize)
        segmentLength = fileSize - fileOffset;

    const char * f = (char*)inputFile.map(fileOffset, segmentLength);
    quint64 wptr = 0;
    QByteArray header;
    header.append(DataPacket);
    header.append(FailsafeTransferProtocol);
    QByteArray data(QByteArray::fromRawData(f, segmentLength));
    while (wptr < segmentLength)
    {
        QByteArray *packet = new QByteArray(header);
        packet->append(toQByteArray((quint64)(fileOffset + wptr)));
        packet->append(TTH);
        //qDebug() << "Write data fileOffset " << fileOffset << " segmentLength " << segmentLength << " wptr " << wptr;
        if (wptr + PACKET_DATA_MTU < segmentLength)
        {
            packet->append(data.mid(wptr, PACKET_DATA_MTU));
            wptr += PACKET_DATA_MTU;
        }
        else
        {
            packet->append(data.mid(wptr, segmentLength - wptr));
            wptr += segmentLength - wptr;
        }
        emit transmitDatagram(remoteHost, packet);
    }
    inputFile.unmap((unsigned char *)f);
}

void FSTPTransferSegment::startDownloading()
{
    checkSendDownloadRequest(FailsafeTransferProtocol, remoteHost, TTH, requestingOffset, requestingLength);
    status = TRANSFER_STATE_RUNNING;
}

inline void FSTPTransferSegment::checkSendDownloadRequest(quint8 protocol, QHostAddress peer, QByteArray TTH,
                                                       quint64 requestingOffset, quint64 requestingLength)
{
    if (fileSize < requestingOffset + requestingLength)
        requestingLength = fileSize - requestingOffset;
    emit sendDownloadRequest(protocol, peer, TTH, requestingOffset, requestingLength);
}

void FSTPTransferSegment::incomingDataPacket(quint64 offset, QByteArray data)
{
    if (offset != requestingOffset)
    {
        status = TRANSFER_STATE_STALLED;
        return;
    }
    status = TRANSFER_STATE_RUNNING;

    int bucketNumber = calculateBucketNumber(offset);
    if (!pDownloadBucketTable->contains(bucketNumber))
    {
        QByteArray *bucket = new QByteArray();
        pDownloadBucketTable->insert(bucketNumber, bucket);
    }
    if ((pDownloadBucketTable->value(bucketNumber)->length() + data.length()) > HASH_BUCKET_SIZE)
    {
        int bucketRemaining = HASH_BUCKET_SIZE - pDownloadBucketTable->value(bucketNumber)->length();
        pDownloadBucketTable->value(bucketNumber)->append(data.mid(0, bucketRemaining));
        if (!pDownloadBucketTable->contains(bucketNumber + 1))
        {
            QByteArray *nextBucket = new QByteArray(data.mid(bucketRemaining));
            pDownloadBucketTable->insert(bucketNumber + 1, nextBucket);
        }
        // there should be no else - if the next bucket exists and data is sticking over, there is an error,
        // since we segment on bucket boundaries. tth checksumming will catch the problems.
    }
    else
        pDownloadBucketTable->value(bucketNumber)->append(data);

    if (pDownloadBucketTable->value(bucketNumber)->length() == HASH_BUCKET_SIZE)
    {
        emit hashBucketRequest(TTH, bucketNumber, pDownloadBucketTable->value(bucketNumber));
    }

    if ((bucketNumber == lastBucketNumber) && (lastBucketSize == pDownloadBucketTable->value(bucketNumber)->length()))
    {
        status = TRANSFER_STATE_FINISHED;  // local segment
        emit hashBucketRequest(TTH, bucketNumber, pDownloadBucketTable->value(bucketNumber));
        return;
    }

    requestingOffset += data.length();
    if (requestingOffset == requestingTargetOffset)
    {
        if (requestingLength < TRANSFER_MAXIMUM_SEGMENT / 2)
            requestingLength *= 2;

        requestingTargetOffset += requestingLength;
        checkSendDownloadRequest(FailsafeTransferProtocol, remoteHost, TTH, requestingOffset, requestingLength);
    }
}

void FSTPTransferSegment::transferTimerEvent()
{
    if (status == TRANSFER_STATE_STALLED)
        {
            // Transfer some data
            if (requestingLength > PACKET_DATA_MTU)
                requestingLength /= 2;

            status = TRANSFER_STATE_RUNNING;
            requestingTargetOffset = requestingOffset + requestingLength;
            //qDebug() << "sendDownloadRequest() peer tth offset length " << listOfPeers.first() << TTH << requestingOffset << requestingLength;
            checkSendDownloadRequest(FailsafeTransferProtocol, remoteHost, TTH, requestingOffset, requestingLength);
        }

}

inline int FSTPTransferSegment::calculateBucketNumber(quint64 fileOffset)
{
    return (int)(fileOffset >> 20);
}

