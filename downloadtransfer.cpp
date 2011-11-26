#include "downloadtransfer.h"

DownloadTransfer::DownloadTransfer()
{
    transferRate = 0;
    transferProgress = 0;
    bytesWrittenSinceUpdate = 0;
    requestingOffset = 0;
    requestingLength = 65536;
    requestingTargetOffset = 0;
    status = TRANSFER_STATE_INITIALIZING;
    remoteHost = QHostAddress("0.0.0.0");

    transferRateCalculationTimer = new QTimer(this);
    connect(transferRateCalculationTimer, SIGNAL(timeout()), this, SLOT(transferRateCalculation()));
    transferRateCalculationTimer->setSingleShot(false);
    transferRateCalculationTimer->start(1000);

    transferTimer = new QTimer(this);
    connect(transferTimer, SIGNAL(timeout()), this, SLOT(transferTimerEvent()));
    transferTimer->setSingleShot(false);
}

DownloadTransfer::~DownloadTransfer()
{
    delete transferRateCalculationTimer;
    delete transferTimer;
    QHashIterator<int, QByteArray*> itdb(downloadBucketTable);
    while (itdb.hasNext())
    {
        // TODO: save halwe buckets na files toe
        delete itdb.next();
    }
    QHashIterator<int, QByteArray*> ithb(downloadBucketHashLookupTable);
    while (ithb.hasNext())
        delete ithb.next();
}

void DownloadTransfer::incomingDataPacket(quint8 transferPacketType, quint64 &offset, QByteArray &data)
{
    // we are interested in which transfer protocol the packet is encoded with, since a download object may receive packets from various sources
    // transmitted with different transfer protocols.

    // we can later break this up into protocols, currently I just want to see it working.
    if (offset != requestingOffset)
        return;

    int bucketNumber = calculateBucketNumber(offset);
    if (!downloadBucketTable.contains(bucketNumber))
    {
        QByteArray *bucket = new QByteArray();
        downloadBucketTable.insert(bucketNumber, bucket);
    }
    if ((downloadBucketTable.value(bucketNumber)->length() + data.length()) > BUCKET_SIZE)
    {
        int bucketRemaining = BUCKET_SIZE - downloadBucketTable.value(bucketNumber)->length();
        downloadBucketTable.value(bucketNumber)->append(data.mid(0, bucketRemaining));
        if (!downloadBucketTable.contains(bucketNumber + 1))
        {
            QByteArray *nextBucket = new QByteArray(data.mid(bucketRemaining));
            downloadBucketTable.insert(bucketNumber + 1, nextBucket);
        }
        // there should be no else - if the next bucket exists and data is sticking over, there is an error,
        // since we segment on bucket boundaries. tth checksumming will catch the problems.
    }
    else
        downloadBucketTable.value(bucketNumber)->append(data);

    if (downloadBucketTable.value(bucketNumber)->length() == BUCKET_SIZE)
        emit hashBucketRequest(TTH, bucketNumber, downloadBucketTable.value(bucketNumber));

    requestingOffset += data.length();
    if (requestingOffset == requestingTargetOffset)
    {
        if (requestingLength < TRANSFER_MAXIMUM_SEGMENT / 2)
            requestingLength *= 2;

        requestingTargetOffset += requestingLength;
        emit sendDownloadRequest(protocolPreference, listOfPeers.first(), TTH, requestingOffset, requestingLength);
    }
}

void DownloadTransfer::hashBucketReply(int &bucketNumber, QByteArray &bucketTTH)
{
    if (downloadBucketHashLookupTable.contains(bucketNumber))
    {
        if (*downloadBucketHashLookupTable.value(bucketNumber) == bucketTTH)
            flushBucketToDisk(bucketNumber);
        else
        {
            // TODO: emit MISTAKE!
        }
    }
    // TODO: must check that tth tree item was received before requesting bucket hash.
}

void DownloadTransfer::TTHTreeReply(QByteArray &tree)
{
    while (tree.length() >= 28)
    {
        int bucketNumber = tree.mid(0, 4).toInt();
        QByteArray *bucketHash = new QByteArray(tree.mid(4, 24));
        tree.remove(0, 28);
        downloadBucketHashLookupTable.insert(bucketNumber, bucketHash);
    }
}

int DownloadTransfer::getTransferType()
{
    return TRANSFER_TYPE_DOWNLOAD;
}

void DownloadTransfer::startTransfer()
{
    transferTimer->start(100);
}

void DownloadTransfer::pauseTransfer()
{
    status = TRANSFER_STATE_PAUSED;  //TODO
}

void DownloadTransfer::abortTransfer()
{
    status = TRANSFER_STATE_ABORTING;
    emit abort(this);
}

// currently copied from uploadtransfer.
// we need the freedom to change this if necessary, therefore, it is not implemented in parent class.
void DownloadTransfer::transferRateCalculation()
{
    if ((status == TRANSFER_STATE_RUNNING) && (bytesWrittenSinceUpdate == 0))
        status = TRANSFER_STATE_STALLED;
    else if (status == TRANSFER_STATE_STALLED)
        status = TRANSFER_STATE_RUNNING;

    // snapshot the transfer rate as the amount of bytes written in the last second
    transferRate = bytesWrittenSinceUpdate;
    bytesWrittenSinceUpdate = 0;
}

int DownloadTransfer::calculateBucketNumber(quint64 fileOffset)
{
    return (int)(fileOffset >> 20);
}

void DownloadTransfer::flushBucketToDisk(int &bucketNumber)
{
    // TODO: decide where to store these files
    QString tempFileName;
    tempFileName.append(TTHBase32);
    tempFileName.append(".");
    tempFileName.append(QString::number(bucketNumber));

    QFile file(tempFileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(*downloadBucketTable.value(bucketNumber));
    else
    {
        // TODO: emit MISTAKE!, pause download
    }
    file.close();
}

// We keep the transfer alive in here
void DownloadTransfer::transferTimerEvent()
{
    if (status == TRANSFER_STATE_INITIALIZING)
    {
        // Get peers and TTH tree
        if (!listOfPeers.isEmpty())
        {
            // simple and stupid for now...
            emit TTHTreeRequest(listOfPeers.first(), TTH);
        }
    }
    else if (status == TRANSFER_STATE_STALLED)
    {
        // Transfer some data
        if (requestingLength > PACKET_DATA_MTU)
            requestingLength /= 2;

        status = TRANSFER_STATE_RUNNING;
        requestingTargetOffset = requestingOffset + requestingLength;
        emit sendDownloadRequest(protocolPreference, listOfPeers.first(), TTH, requestingOffset, requestingLength);
    }

}

void DownloadTransfer::setProtocolPreference(QByteArray &preference)
{
    protocolPreference = preference;
}
