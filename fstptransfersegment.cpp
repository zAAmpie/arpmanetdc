#include "fstptransfersegment.h"

FSTPTransferSegment::FSTPTransferSegment(Transfer *parent) : TransferSegment(parent)
{
    status = TRANSFER_STATE_INITIALIZING;
    prev_status = -1;
    requestingOffset = 0;
    //requestingLength = 65536;
    requestingLength = 131072;
    requestingTargetOffset = 0;
    retransmitTimeoutCounter = 0;
    retransmitRetryCounter = 0;
    packetsSinceUpdate = 0;

    pParent = parent;
}

FSTPTransferSegment::~FSTPTransferSegment()
{
    if (inputFile.isOpen())
        inputFile.close();

    qDebug() << "FSTPTransferSegment DESTROYING: " << this;
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

void FSTPTransferSegment::startUploading()
{
    maxUploadRequestOffset = 0;

    if (segmentStart > fileSize)
    {
        emit sendTransferError(remoteHost, InvalidOffsetError, TTH, segmentStart);
        return;
    }
    else if (segmentStart + segmentLength > fileSize)
        segmentLength = fileSize - segmentStart;

    //Save offset for upload progress - this might need to be set to an average or median value for multiple segmented 
    //downloading but if segments aren't spaced TOO widely the maximum will give a good estimate of total progress
    maxUploadRequestOffset = maxUploadRequestOffset < segmentStart ? segmentStart : maxUploadRequestOffset;
    if (segmentStart + segmentLength == fileSize)
        maxUploadRequestOffset = fileSize;

    const char * f = (char*)inputFile.map(segmentStart, segmentLength);
    if (f == 0)
    {
        emit sendTransferError(remoteHost, FileIOError, TTH, segmentStart);
        return;
    }

    quint64 wptr = 0;
    QByteArray header;
    header.reserve(2);
    if (segmentId > 0)
        header.append(DirectDataPacket);
    else
        header.append(DataPacket);
    header.append(FailsafeTransferProtocol);
    QByteArray data(QByteArray::fromRawData(f, segmentLength));
    while (wptr < segmentLength)
    {
        QByteArray *packet = new QByteArray(header);
        packet->reserve(PACKET_MTU);
        if (segmentId > 0)
        {
            packet->append(quint64ToByteArray((quint64)(segmentStart + wptr)));
            packet->append(quint32ToByteArray(segmentId));
        }
        else
        {
            packet->append(quint64ToByteArray((quint64)(segmentStart + wptr)));
            packet->append(TTH);
        }
        //qDebug() << "Write data segmentStart " << segmentStart << " segmentLength " << segmentLength << " wptr " << wptr;
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
    if (!(pParent->getTransferStatus() & (TRANSFER_STATE_STALLED | TRANSFER_STATE_RUNNING)))
        return;
    if (status & (TRANSFER_STATE_INITIALIZING | TRANSFER_STATE_FINISHED))
    {
        segmentStartTime = QDateTime::currentMSecsSinceEpoch();
        requestingOffset = segmentStart;
        requestingTargetOffset = requestingOffset + requestingLength;
        retransmitTimeoutCounter = 0;
        retransmitRetryCounter = 0;
        //qDebug() << "FSTPTransferSegment::startDownloading() call checkSendDownloadRequest()";
        status = TRANSFER_STATE_RUNNING;
        checkSendDownloadRequest(FailsafeTransferProtocol, remoteHost, TTH, requestingOffset, requestingLength, status);
    }
}

void FSTPTransferSegment::pauseDownload()
{
    prev_status = status;
    status = TRANSFER_STATE_PAUSED;
}

void FSTPTransferSegment::unpauseDownload()
{
    if (prev_status != -1)
        status = prev_status;
}

void FSTPTransferSegment::incomingDataPacket(quint64 offset, QByteArray data)
{
    //Ignore packet if transfer has failed and has not been restarted
    if (status == TRANSFER_STATE_FAILED && offset != segmentStart)
        return;

    if (offset > requestingOffset)
    {
        status = TRANSFER_STATE_STALLED;
        return;
    }
    if (offset < requestingOffset)
        return;

    int b = data.length();
    emit updateDirectBytesStats(b);
    bytesTransferred += b;

    status = TRANSFER_STATE_RUNNING;
    packetsSinceUpdate++;
    retransmitRetryCounter = 0;

    int bucketNumber = calculateBucketNumber(offset);
    if (!pDownloadBucketTable->contains(bucketNumber))
    {
        QByteArray *bucket = new QByteArray();
        bucket->reserve(1048576);
        pDownloadBucketTable->insert(bucketNumber, bucket);
    }
    if ((pDownloadBucketTable->value(bucketNumber)->length() + data.length()) > HASH_BUCKET_SIZE)
    {
        int bucketRemaining = HASH_BUCKET_SIZE - pDownloadBucketTable->value(bucketNumber)->length();
        pDownloadBucketTable->value(bucketNumber)->append(data.mid(0, bucketRemaining));
        if (!pDownloadBucketTable->contains(bucketNumber + 1))
        {
            QByteArray *nextBucket = new QByteArray(data.mid(bucketRemaining));
            nextBucket->reserve(1048576);
            pDownloadBucketTable->insert(bucketNumber + 1, nextBucket);
        }
        // there should be no else - if the next bucket exists and data is sticking over, there is an error,
        // since we segment on bucket boundaries. tth checksumming will catch the problems.
    }
    else
    {
        pDownloadBucketTable->value(bucketNumber)->append(data);
        //qDebug() << "Append data " << requestingOffset << offset << pDownloadBucketTable->value(bucketNumber)->length();
    }

    if (pDownloadBucketTable->value(bucketNumber)->length() == HASH_BUCKET_SIZE)
    {
        emit hashBucketRequest(TTH, bucketNumber, pDownloadBucketTable->value(bucketNumber));
        //qDebug() << "FSTPTransferSegment emit hashBucketRequest() " << bucketNumber << pDownloadBucketTable->value(bucketNumber)->length();
    }

    // these last bucket numbers are for the *segment*, not the file.
    // the length check is for in case it is also the last segment of the file.
    if ((bucketNumber == lastBucketNumber) && (lastBucketSize == pDownloadBucketTable->value(bucketNumber)->length())) // End of Segment
    {
        status = TRANSFER_STATE_FINISHED;  // local segment
        //qDebug() << "FSTPTransferSegment emit hashBucketRequest() on finish " << bucketNumber << pDownloadBucketTable->value(bucketNumber)->length();
        emit hashBucketRequest(TTH, bucketNumber, pDownloadBucketTable->value(bucketNumber));
    }

    requestingOffset += data.length();
    if ((requestingOffset == requestingTargetOffset) && (requestingOffset < segmentEnd))
    {
        if (requestingLength <= FSTP_TRANSFER_MAXIMUM_SEGMENT / 2)
            requestingLength *= 2;

        requestingTargetOffset += requestingLength;
        //qDebug() << "FSTPTransferSegment::incomingDataPacket() call checkSendDownloadRequest()";
        checkSendDownloadRequest(FailsafeTransferProtocol, remoteHost, TTH, requestingOffset, requestingLength, status);
    }

    if (requestingOffset >= segmentEnd)
    {
        emit requestNextSegment(this);
    }
}

void FSTPTransferSegment::transferTimerEvent()
{
    if (!(pParent->getTransferStatus() & (TRANSFER_STATE_STALLED | TRANSFER_STATE_RUNNING)))
        return;
    if (status == TRANSFER_STATE_STALLED)
    {
        // Transfer some data
        if (requestingLength > FSTP_TRANSFER_MINIMUM_SEGMENT)
            requestingLength /= 2;
        status = TRANSFER_STATE_RUNNING;
        requestingTargetOffset = requestingOffset + requestingLength;
        //qDebug() << "FSTPTransferSegment::transferTimerEvent() call checkSendDownloadRequest()" << requestingOffset << requestingLength;
        checkSendDownloadRequest(FailsafeTransferProtocol, remoteHost, TTH, requestingOffset, requestingLength, status);
        retransmitRetryCounter++;
        // retransmit timeout 2 seconds, 5 retransmits / 10 seconds deadness plenty enough to warrant a fail.
        if (retransmitRetryCounter == 5)
        {
            status = TRANSFER_STATE_FAILED;
            emit transferRequestFailed(this);
        }
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
        {
            packetsSinceUpdate = 0;
            //By adding this we only set a transfer as STALLED when 20 *sequential* updates produced no packets, vs just 20 overall
            retransmitTimeoutCounter = 0;
        }
    }
}
