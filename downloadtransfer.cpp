#include "downloadtransfer.h"

DownloadTransfer::DownloadTransfer()
{
    transferRate = 0;
    transferProgress = 0;
    bytesWrittenSinceUpdate = 0;
    status = TRANSFER_STATE_INITIALIZING;
    remoteHost = QHostAddress("0.0.0.0");

    transferRateCalculationTimer = new QTimer(this);
    connect(transferRateCalculationTimer, SIGNAL(timeout()), this, SLOT(transferRateCalculation()));
    transferRateCalculationTimer->setSingleShot(false);
    transferRateCalculationTimer->start(1000);
}

DownloadTransfer::~DownloadTransfer()
{
    delete transferRateCalculationTimer;
    QHashIterator<int, QByteArray*> it(downloadBucketTable);
    while (it.hasNext())
    {
        // TODO: save halwe buckets na files toe
        delete it.next();
    }
}

void DownloadTransfer::incomingDataPacket(quint8 transferProtocolVersion, quint64 &offset, QByteArray &data)
{
    // we are interested in which transfer protocol the packet is encoded with, since a download object may receive packets from various sources
    // transmitted with different transfer protocols.
    int bucketNumber = calculateBucketNumber(offset);
    if (!downloadBucketTable.contains(bucketNumber))
    {
        QByteArray *bucket = new QByteArray();
        downloadBucketTable.insert(bucketNumber, bucket);
    }
    if (downloadBucketTable.value(bucketNumber)->length() + data.length() > BUCKET_SIZE)
    {
        int bucketRemaining = BUCKET_SIZE - downloadBucketTable.value(bucketNumber)->length();
        downloadBucketTable.value(bucketNumber)->append(data.mid(0, bucketRemaining));
        if (!downloadBucketTable.contains(bucketNumber + 1))
        {
            QByteArray *nextBucket = new QByteArray(data.mid(bucketRemaining));
            downloadBucketTable.insert(bucketNumber + 1, nextBucket);
        }
        // there should be no else - if the next bucket exists and data is sticking over, there is an error, since we segment on bucket boundaries.
        // tth checksumming will catch the problems.
    }
    else
        downloadBucketTable.value(bucketNumber)->append(data);

    if (downloadBucketTable.value(bucketNumber)->length() == BUCKET_SIZE)
        emit hashBucketRequest(TTH, bucketNumber, downloadBucketTable.value(bucketNumber));
}

void DownloadTransfer::hashBucketReply(QByteArray &rootTTH, int &bucketNumber, QByteArray &bucketTTH)
{
    // TODO: get tree, compare, flush functions
}

int DownloadTransfer::getTransferType()
{
    return TRANSFER_TYPE_DOWNLOAD;
}

void DownloadTransfer::startTransfer()
{
    status = TRANSFER_STATE_PAUSED; //TODO
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
