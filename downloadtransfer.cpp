#include "downloadtransfer.h"
#include <QThread>

DownloadTransfer::DownloadTransfer(QObject *parent) : Transfer(parent)
{
    //TODO: load transferSegmentStateBitmap from db
    downloadBucketTable = new QHash<int, QByteArray*>();
    transferRate = 0;
    transferProgress = 0;
    bytesWrittenSinceUpdate = 0;
    initializationStateTimerBrakes = 0;
    status = TRANSFER_STATE_INITIALIZING;
    remoteHost = QHostAddress("0.0.0.0");
    currentActiveSegments = 0;

    transferRateCalculationTimer = new QTimer();
    connect(transferRateCalculationTimer, SIGNAL(timeout()), this, SLOT(transferRateCalculation()));
    transferRateCalculationTimer->setSingleShot(false);
    transferRateCalculationTimer->start(1000);

    transferTimer = new QTimer();
    connect(transferTimer, SIGNAL(timeout()), this, SLOT(transferTimerEvent()));
    transferTimer->setSingleShot(false);

    TTHSearchTimer = new QTimer();
    connect(TTHSearchTimer, SIGNAL(timeout()), this, SLOT(TTHSearchTimerEvent()));
    transferTimer->setSingleShot(false);
    transferTimer->start(300000);  // every 5 minutes
}

DownloadTransfer::~DownloadTransfer()
{
    //TODO: save transferSegmentStateBitmap to db
    transferRateCalculationTimer->deleteLater();
    transferTimer->deleteLater();
    QHashIterator<int, QByteArray*> itdb(*downloadBucketTable);
    while (itdb.hasNext())
    {
        // TODO: save halwe buckets na files toe
        delete itdb.next().value();
    }
    delete downloadBucketTable;

    QMapIterator<int, QByteArray*> ithb(downloadBucketHashLookupTable);
    while (ithb.hasNext())
        delete ithb.next().value();

    QHashIterator<QHostAddress, RemotePeerInfoStruct> r(remotePeerInfoTable);
    while (r.hasNext())
        if (r.next().value().transferSegment)
            r.value().transferSegment->deleteLater();
}

void DownloadTransfer::incomingDataPacket(quint8, quint64 offset, QByteArray data)
{
    // map are sorted in key ascending order
    // QMap::lowerBound will most likely find the segment just before the one we are interested in
    QMap<quint64, TransferSegmentTableStruct>::const_iterator i = transferSegmentTable.upperBound(offset);
    if (Q_UNLIKELY(i == transferSegmentTable.constEnd()))
        --i;
    if (Q_UNLIKELY(i.key() <= offset && i.value().segmentEnd >= offset))
        i.value().transferSegment->incomingDataPacket(offset, data);
    else if (Q_LIKELY(i != transferSegmentTable.constBegin()))
    {
        --i;
        if (Q_LIKELY(i.key() <= offset && i.value().segmentEnd >= offset))
            i.value().transferSegment->incomingDataPacket(offset, data);
    }
    // if the offset was not found like this, it probably is not in the map.

    /*while (i.hasPrevious())
    {
        i.previous();
        if (i.key() <= offset && i.value().segmentEnd >= offset)
        {

            break;
        }

    }*/
    bytesWrittenSinceUpdate += data.size();
}

void DownloadTransfer::requestHashBucket(QByteArray rootTTH, int bucketNumber, QByteArray *bucket)
{
    if (bucketFlushStateBitmap.at(bucketNumber) == BucketNotFlushed)
    {
        emit hashBucketRequest(rootTTH, bucketNumber, *bucket);
        bucketFlushStateBitmap[bucketNumber] = BucketFlushed;
    }
}

void DownloadTransfer::hashBucketReply(int bucketNumber, QByteArray bucketTTH)
{
    // buckets can be flushed twice when aligned with a segment boundary.
    // a race condition between two threads can cause segmentation faults
    // here we make sure we attempt to flush the buffer only once.
    if (downloadBucketHashLookupTable.contains(bucketNumber))
    {
        if (*downloadBucketHashLookupTable.value(bucketNumber) == bucketTTH)
        {
            flushBucketToDisk(bucketNumber);
        }
        else
        {
            transferSegmentStateBitmap[bucketNumber] = SegmentNotDownloaded;
            bucketFlushStateBitmap[bucketNumber] = BucketNotFlushed;
            // TODO: emit MISTAKE!
        }
    }
    // TODO: must check that tth tree item was received before requesting bucket hash.
}

void DownloadTransfer::TTHTreeReply(QByteArray tree)
{
    int iter = 0;
    while (tree.length() >= 29)
    {
        int bucketNumber = getQuint32FromByteArray(&tree);
        quint8 tthLength = getQuint8FromByteArray(&tree);
        QByteArray *bucketHash = new QByteArray(tree.left(tthLength));
        tree.remove(0, tthLength);
        if (!downloadBucketHashLookupTable.contains(bucketNumber))
        {
            downloadBucketHashLookupTable.insert(bucketNumber, bucketHash);
            iter++;
        }
    }
    if (iter == 0)  // don't burn it if the buckets start showing up empty or if we receive duplicates
        return;

    int prev = -1;
    QMapIterator<int, QByteArray*> i(downloadBucketHashLookupTable);
    while (i.hasNext())
    {
        i.next();
        if (i.key() == prev + 1)
            prev++;
        else
            break;
    }
    emit TTHTreeRequest(listOfPeers.first(), TTH, prev + 1, 184);
    qDebug() << "Request TTH tree " << prev + 1 << calculateBucketNumber(fileSize);
}

int DownloadTransfer::getTransferType()
{
    return TRANSFER_TYPE_DOWNLOAD;
}

void DownloadTransfer::startTransfer()
{
    //QThread *thisThread = QThread::currentThread();
    //QThread *thatThread = transferTimer->thread();
    lastBucketNumber = calculateBucketNumber(fileSize);
    lastBucketSize = fileSize % HASH_BUCKET_SIZE;
    if (transferSegmentStateBitmap.length() == 0)
        for (int i = 0; i <= lastBucketNumber; i++)
            transferSegmentStateBitmap.append(SegmentNotDownloaded);
    if (bucketFlushStateBitmap.length() == 0)
        for (int i = 0; i <= lastBucketNumber; i++)
            bucketFlushStateBitmap.append(BucketNotFlushed);

    transferTimer->start(100);
    timerBrakes = 0;
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

void DownloadTransfer::addPeer(QHostAddress peer)
{
    if (peer.toIPv4Address() > 0 && !listOfPeers.contains(peer))
    {
        listOfPeers.append(peer);  // TODO: is this list then strictly necessary?
        emit requestProtocolCapability(peer, this);
    }
}

// currently copied from uploadtransfer.
// we need the freedom to change this if necessary, therefore, it is not implemented in parent class.
void DownloadTransfer::transferRateCalculation()
{
    if ((status == TRANSFER_STATE_RUNNING) && (bytesWrittenSinceUpdate == 0))
        status = TRANSFER_STATE_STALLED;
    else if ((status == TRANSFER_STATE_STALLED) && (bytesWrittenSinceUpdate > 0))
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
    transferSegmentStateBitmap[bucketNumber] = SegmentDownloaded;

    int segmentsDone = 0;
    for (int i = 0; i < transferSegmentStateBitmap.length(); i++)
    {
        if (transferSegmentStateBitmap.at(i) == SegmentDownloaded)
            segmentsDone++;
    }
    int fileBuckets = calculateBucketNumber(fileSize);
    fileBuckets = fileSize % HASH_BUCKET_SIZE == 0 ? fileBuckets : fileBuckets + 1;
    if (segmentsDone ==  fileBuckets)
    {
        status = TRANSFER_STATE_FINISHED;
        emit transferFinished(TTH);
        abortTransfer();
    }
}

void DownloadTransfer::transferTimerEvent()
{
    if (status == TRANSFER_STATE_INITIALIZING)
    {
        timerBrakes++;
        if (timerBrakes > 1)
        {
            if (timerBrakes == 50)
                timerBrakes = 0;
            return;
        }
        int lastHashBucketReceived = -1;
        if (!downloadBucketHashLookupTable.isEmpty())
        {
            QMap<int, QByteArray*>::const_iterator i = downloadBucketHashLookupTable.constEnd();
            --i;
            lastHashBucketReceived = i.key();
        }

        int lastBucket = calculateBucketNumber(fileSize);
        lastBucket = fileSize % HASH_BUCKET_SIZE == 0 ? lastBucket - 1 : lastBucket;
        if (lastHashBucketReceived == lastBucket)
        {
            status = TRANSFER_STATE_RUNNING;
            QHashIterator<QHostAddress, RemotePeerInfoStruct> i(remotePeerInfoTable);
            while (i.hasNext())
            {
                i.next();
                if (i.value().transferSegment)
                    i.value().transferSegment->startDownloading();
            }
        }
        else
        {
            qDebug() << "Timer request TTH tree " << listOfPeers.first().toString() << lastHashBucketReceived + 1;
            emit TTHTreeRequest(listOfPeers.first(), TTH, lastHashBucketReceived + 1, 184); // 8 datagrams
        }
    }
}

/*void DownloadTransfer::setProtocolPreference(QByteArray &preference)
{
    protocolPreference = preference;
}*/

inline int DownloadTransfer::calculateBucketNumber(quint64 fileOffset)
{
    return (int)(fileOffset >> 20);
}

void DownloadTransfer::segmentCompleted(TransferSegment *segment)
{
    qint64 lastTransferTime = QDateTime::currentMSecsSinceEpoch() - segment->getSegmentStartTime();
    int lastSegmentLength = (segment->getSegmentEnd() - segment->getSegmentStart()) / HASH_BUCKET_SIZE;
    int nextSegmentLengthHint = (10000 / lastTransferTime) * lastSegmentLength;
    if (nextSegmentLengthHint == 0)
        nextSegmentLengthHint = 1;

    transferSegmentTable.remove(segment->getSegmentStart());
    downloadNextAvailableChunk(segment, nextSegmentLengthHint);
}

void DownloadTransfer::segmentFailed(TransferSegment *segment)
{
    // remote end dead, segment given up hope. mark everything not downloaded as not downloaded, so that
    // the block allocator can give them to other segments that do work.
    // do not worry if this is the last working segment, if it breaks, it can resume on successful TTH search reply.
    int startBucket = calculateBucketNumber(segment->getSegmentStart());
    int endBucket = calculateBucketNumber(segment->getSegmentEnd());
    for (int i = startBucket; i <= endBucket; i++)
        if (transferSegmentStateBitmap.at(i) == SegmentCurrentlyDownloading)
            transferSegmentStateBitmap[i] = SegmentNotDownloaded;

    transferSegmentTable.remove(segment->getSegmentStart());
    currentActiveSegments--;
    // currently the object keeps sitting in remotePeerInfoTable and gets destroyed once the download completes.
    // this can be improved (TODO)
}

SegmentOffsetLengthStruct DownloadTransfer::getSegmentForDownloading(int segmentNumberOfBucketsHint)
{
    // Ideas for quick and dirty block allocator:
    // Scan once over bitmap, taking note of starting point and length of longest open segment
    // Allocate block immediately if long enough gap found, otherwise allocate longest possible gap
    SegmentOffsetLengthStruct segment;
    int longestSegmentStart = 0;
    int longestSegmentLength = 0;
    int currentSegmentStart = 0;
    int currentSegmentLength = 0;
    char lastSegmentState = SegmentDownloaded;
    for (int i = 0; i < transferSegmentStateBitmap.length(); i++)
    {
        if (transferSegmentStateBitmap.at(i) == SegmentNotDownloaded)
        {
            if (lastSegmentState != SegmentNotDownloaded)
            {
                currentSegmentStart = i;
                currentSegmentLength = 1;
            }
            else
            {
                currentSegmentLength++;
            }
            if (currentSegmentLength == segmentNumberOfBucketsHint)
            {
                segment.segmentBucketOffset = currentSegmentStart;
                segment.segmentBucketCount = currentSegmentLength;
                for (int j = currentSegmentStart; j < currentSegmentStart+currentSegmentLength; j++)
                    transferSegmentStateBitmap[j] = SegmentCurrentlyDownloading;
                return segment;
            }
            if (currentSegmentLength > longestSegmentLength)
            {
                longestSegmentStart = currentSegmentStart;
                longestSegmentLength = currentSegmentLength;
            }
        }
        lastSegmentState = transferSegmentStateBitmap.at(i);
    }
    segment.segmentBucketOffset = longestSegmentStart;
    segment.segmentBucketCount = longestSegmentLength;
    for (int j = longestSegmentStart; j < longestSegmentStart+longestSegmentLength; j++)
        transferSegmentStateBitmap[j] = SegmentCurrentlyDownloading;
    return segment;
}

void DownloadTransfer::receivedPeerProtocolCapability(QHostAddress peer, quint8 protocols)
{
    newPeer(peer, protocols);
    if (currentActiveSegments < MAXIMUM_SIMULTANEOUS_SEGMENTS)
    {
        currentActiveSegments++;
        TransferSegment *download = createTransferSegment(peer);
        if (download)
            downloadNextAvailableChunk(download);
    }
}

void DownloadTransfer::newPeer(QHostAddress peer, quint8 protocols)
{
    if (!remotePeerInfoTable.contains(peer))
    {
        RemotePeerInfoStruct rpis;
        rpis.bytesTransferred = 0;
        rpis.protocolCapability = protocols;
        rpis.transferSegment = 0;
        remotePeerInfoTable.insert(peer, rpis);
    }
}

TransferSegment* DownloadTransfer::createTransferSegment(QHostAddress peer)
{
    TransferSegment *download = 0;
    for (int i = 0; i < protocolOrderPreference.length(); i++)
    {
        if (remotePeerInfoTable.value(peer).triedProtocols.contains(protocolOrderPreference.at(i)))
            continue;
        if (protocolOrderPreference.at(i) & remotePeerInfoTable.value(peer).protocolCapability)
        {
            TransferProtocol p = (TransferProtocol)protocolOrderPreference.at(i);
            remotePeerInfoTable[peer].triedProtocols.append(p);
            download  = newConnectedTransferSegment(p);
            remotePeerInfoTable[peer].transferSegment = download;
            download->setDownloadBucketTablePointer(downloadBucketTable);
            download->setRemoteHost(peer);
            download->setTTH(TTH);
            download->setFileSize(fileSize);
            break;
        }
    }
    return download;
}

TransferSegment* DownloadTransfer::newConnectedTransferSegment(TransferProtocol p)
{
    TransferSegment* download = 0;
    switch(p)
    {
    case FailsafeTransferProtocol:
        download = new FSTPTransferSegment(this);
        break;
    case BasicTransferProtocol:
    case uTPProtocol:
    case ArpmanetFECProtocol:
        break;
    }
    connect(download, SIGNAL(hashBucketRequest(QByteArray,int,QByteArray*)), this, SLOT(requestHashBucket(QByteArray,int,QByteArray*)));
    connect(download, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,quint64,quint64)),
            this, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,quint64,quint64)));
    connect(download, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)), this, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)));
    connect(transferTimer, SIGNAL(timeout()), download, SLOT(transferTimerEvent()));
    connect(download, SIGNAL(requestNextSegment(TransferSegment*)), this, SLOT(segmentCompleted(TransferSegment*)));
    connect(download, SIGNAL(transferRequestFailed(TransferSegment*)), this, SLOT(segmentFailed(TransferSegment*)));
    //connect(download, SIGNAL(requestPeerProtocolCapability(QHostAddress,Transfer*)), this, SIGNAL(requestProtocolCapability(QHostAddress,Transfer*)));
    return download;
}

void DownloadTransfer::downloadNextAvailableChunk(TransferSegment *download, int length)
{
    TransferSegmentTableStruct t;
    SegmentOffsetLengthStruct s = getSegmentForDownloading(length);
    quint64 segmentStart = s.segmentBucketOffset * HASH_BUCKET_SIZE;
    t.segmentEnd = (s.segmentBucketOffset + s.segmentBucketCount) * HASH_BUCKET_SIZE;
    t.segmentEnd = t.segmentEnd > fileSize ? fileSize : t.segmentEnd;
    download->setSegmentStart(segmentStart);
    download->setSegmentEnd(t.segmentEnd);
    if (segmentStart != t.segmentEnd) // otherwise done
    {
        t.transferSegment = download;
        transferSegmentTable.insert(segmentStart, t);
        download->startDownloading();
    }
}

void DownloadTransfer::TTHSearchTimerEvent()
{
    if (currentActiveSegments < MAXIMUM_SIMULTANEOUS_SEGMENTS)
        emit searchTTHAlternateSources(TTH);
}

int DownloadTransfer::getTransferProgress()
{
    //===== 1 MB precision progress =====
    int segmentsDone = 0;
    for (int i = 0; i < transferSegmentStateBitmap.length(); i++)
    {
        if (transferSegmentStateBitmap.at(i) == SegmentDownloaded)
            segmentsDone++;
    }

    //Clause to only use byte precision for files smaller than 100 MB
    if (fileSize >= 100*(1<<20))
        return (int)(((double)segmentsDone / (calculateBucketNumber(fileSize) + 1)) * 100);

    //===== 1 byte precision progress =====
    qint64 dataReceivedNotFlushed = 0;
    int segmentsActive = 0;

    //Go through all active segments and check the amount of data received
    foreach (TransferSegmentTableStruct t, transferSegmentTable)
    {
        if (t.transferSegment)
        {
            dataReceivedNotFlushed += t.transferSegment->getBytesReceivedNotFlushed();
            segmentsActive++;
        }
    }
            
    qint64 bytesDone = segmentsDone * HASH_BUCKET_SIZE + dataReceivedNotFlushed;
    int returnVal = bytesDone * 100 / fileSize;
    
    return returnVal > 100 ? 100 : returnVal;
}
