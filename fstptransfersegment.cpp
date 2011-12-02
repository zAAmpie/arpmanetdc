#include "fstptransfersegment.h"

FSTPTransferSegment::FSTPTransferSegment(Transfer *parent) : TransferSegment(parent)
{
    requestingOffset = 0;
    //requestingLength = 65536;
    requestingLength = 131072;
    requestingTargetOffset = 0;
    retransmitTimeoutCounter = 0;

    pParent = parent;
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

void FSTPTransferSegment::setFileSize(quint64 size)
{
    fileSize = size;
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
                                                       quint64 requestingOffset, qint64 requestingLength)
{
    if (fileSize < requestingOffset + requestingLength)
        requestingLength = fileSize - requestingOffset;
    if (requestingLength > 0)
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
    packetsSinceUpdate++;

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

    // these last bucket numbers are for the *segment*, not the file.
    // the length check is for in case it is also the last segment of the file.
    if ((bucketNumber == lastBucketNumber) && (lastBucketSize == pDownloadBucketTable->value(bucketNumber)->length()))
    {
        status = TRANSFER_STATE_FINISHED;  // local segment
        emit hashBucketRequest(TTH, bucketNumber, pDownloadBucketTable->value(bucketNumber));
        emit requestNextSegment(this);
        return;
    }

    requestingOffset += data.length();
    if (requestingOffset == requestingTargetOffset)
    {
        if (requestingLength <= FSTP_TRANSFER_MAXIMUM_SEGMENT / 2)
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
        if (requestingLength > FSTP_TRANSFER_MINIMUM_SEGMENT)
            requestingLength /= 2;
        status = TRANSFER_STATE_RUNNING;
        requestingTargetOffset = requestingOffset + requestingLength;
        qDebug() << "FSTPTransferSegment:: emit sendDownloadRequest() peer tth offset length " << remoteHost << TTH.toBase64() << requestingOffset << requestingLength;
        checkSendDownloadRequest(FailsafeTransferProtocol, remoteHost, TTH, requestingOffset, requestingLength);
    }
    else if (status == TRANSFER_STATE_RUNNING)
    {
        if (packetsSinceUpdate == 0)
        {
            retransmitTimeoutCounter++;
            if (retransmitTimeoutCounter > 20)
            {
                status = TRANSFER_STATE_STALLED;
                retransmitTimeoutCounter = 0;
            }
        }
        else
            packetsSinceUpdate = 0;
    }
}

inline int FSTPTransferSegment::calculateBucketNumber(quint64 fileOffset)
{
    return (int)(fileOffset >> 20);
}

