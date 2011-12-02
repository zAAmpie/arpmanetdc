#include "downloadtransfer.h"
#include <QThread>

DownloadTransfer::DownloadTransfer(QObject *parent) : Transfer(parent)
{
    downloadBucketTable = new QHash<int, QByteArray*>;
    transferRate = 0;
    transferProgress = 0;
    bytesWrittenSinceUpdate = 0;
    initializationStateTimerBrakes = 0;
    status = TRANSFER_STATE_INITIALIZING;
    remoteHost = QHostAddress("0.0.0.0");

    transferRateCalculationTimer = new QTimer();
    connect(transferRateCalculationTimer, SIGNAL(timeout()), this, SLOT(transferRateCalculation()));
    transferRateCalculationTimer->setSingleShot(false);
    transferRateCalculationTimer->start(1000);

    transferTimer = new QTimer();
    connect(transferTimer, SIGNAL(timeout()), this, SLOT(transferTimerEvent()));
    transferTimer->setSingleShot(false);

    // Temp test
    download = new FSTPTransferSegment(this);
    connect(download, SIGNAL(hashBucketRequest(QByteArray,int,QByteArray*)), this, SIGNAL(hashBucketRequest(QByteArray,int,QByteArray*)));
    connect(download, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,quint64,quint64)),
            this, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,quint64,quint64)));
    connect(download, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)), this, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)));
    connect(transferTimer, SIGNAL(timeout()), download, SLOT(transferTimerEvent()));
}

DownloadTransfer::~DownloadTransfer()
{
    transferRateCalculationTimer->deleteLater();
    transferTimer->deleteLater();
    QHashIterator<int, QByteArray*> itdb(*downloadBucketTable);
    while (itdb.hasNext())
    {
        // TODO: save halwe buckets na files toe
        delete itdb.next().value();
    }
    delete downloadBucketTable;

    //Sover ek verstaan gaan downloadBucketHashLookupTable al uit scope uit voor jy by hierdie destructor kom?
    //So jy moet of hom 'n pointer maak of net hierdie stap heeltemal uithaal
    //A: DownloadTransfer class variable, hy behoort nog hier te wees. ons wil die qbytearray pointers binne-in die ding delete.
    QHashIterator<int, QByteArray*> ithb(downloadBucketHashLookupTable);
    while (ithb.hasNext())
        delete ithb.next().value();

    // tmp
    download->deleteLater();
}

void DownloadTransfer::incomingDataPacket(quint8 transferPacketType, quint64 offset, QByteArray data)
{
    // TODO: select segment object from range and dispatch
    download->incomingDataPacket(offset, data);

    bytesWrittenSinceUpdate += data.size();
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

void DownloadTransfer::TTHTreeReply(QByteArray tree)
{
    while (tree.length() >= 30)
    {
        QByteArray tmp = tree.mid(0, 4);
        int bucketNumber = getQuint32FromByteArray(&tmp);
        tmp = tree.mid(4, 2);
        int tthLength = getQuint16FromByteArray(&tmp);
        QByteArray *bucketHash = new QByteArray(tree.mid(6, tthLength));
        tree.remove(0, 6 + tthLength);
        downloadBucketHashLookupTable.insert(bucketNumber, bucketHash);
    }
}

int DownloadTransfer::getTransferType()
{
    return TRANSFER_TYPE_DOWNLOAD;
}

void DownloadTransfer::startTransfer()
{
    QThread *thisThread = QThread::currentThread();
    QThread *thatThread = transferTimer->thread();
    lastBucketNumber = calculateBucketNumber(fileSize);
    lastBucketSize = fileSize % HASH_BUCKET_SIZE;
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

void DownloadTransfer::flushBucketToDisk(int &bucketNumber)
{
    // TODO: decide where to store these files
    QString tempFileName;
    tempFileName.append(TTHBase32);
    tempFileName.append(".");
    tempFileName.append(QString::number(bucketNumber));

    emit flushBucket(tempFileName, downloadBucketTable->value(bucketNumber));
    emit assembleOutputFile(TTHBase32, filePathName, bucketNumber, lastBucketNumber);

    // just remove entry, bucket pointer gets deleted in BucketFlushThread
    downloadBucketTable->remove(bucketNumber);
}

void DownloadTransfer::transferTimerEvent()
{
    if (status == TRANSFER_STATE_INITIALIZING)
    {
        // this is really primitive
        initializationStateTimerBrakes++;
        if (initializationStateTimerBrakes > 1)
        {
            if (initializationStateTimerBrakes == 100) // 10s with 100ms timer period
                initializationStateTimerBrakes = 0;
            return;
        }

        // Get peers and TTH tree
        if (listOfPeers.isEmpty())
        {
            emit searchTTHAlternateSources(TTH);
        }
        else if (!(downloadBucketHashLookupTable.size() - 1 == lastBucketNumber))
        {
            // simple and stupid for now...
            qDebug() << listOfPeers;
            emit TTHTreeRequest(listOfPeers.first(), TTH);
        }
        else
        {
            status = TRANSFER_STATE_RUNNING;
            download->setFileOffset(0);
            download->setFileOffsetLength(fileSize);
            download->setDownloadBucketTablePointer(downloadBucketTable);
            download->setRemoteHost(listOfPeers.first());
            download->setTTH(TTH);
            download->setLastBucketNumber(lastBucketNumber);
            download->setLastBucketSize(lastBucketSize);
            download->setFileSize(fileSize);
            download->startDownloading();
        }
    }
}

void DownloadTransfer::setProtocolPreference(QByteArray &preference)
{
    protocolPreference = preference;
}

inline int DownloadTransfer::calculateBucketNumber(quint64 fileOffset)
{
    return (int)(fileOffset >> 20);
}

