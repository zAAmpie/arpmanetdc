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
    treeRequestHost = QHostAddress();
    currentActiveSegments = 0;
    hashTreeWindowEnd = 0;
    bucketHashQueueLength = 0;
    bucketFlushQueueLength = 0;
    iowait = false;
    treeUpdatesSinceTimer = 0;

    transferRateCalculationTimer = new QTimer(this);
    connect(transferRateCalculationTimer, SIGNAL(timeout()), this, SLOT(transferRateCalculation()));
    transferRateCalculationTimer->setSingleShot(false);
    transferRateCalculationTimer->start(1000);

    transferTimer = new QTimer(this);
    connect(transferTimer, SIGNAL(timeout()), this, SLOT(transferTimerEvent()));
    transferTimer->setSingleShot(false);
    transferTimer->setInterval(100);

    TTHSearchTimer = new QTimer(this);
    connect(TTHSearchTimer, SIGNAL(timeout()), this, SLOT(TTHSearchTimerEvent()));
    TTHSearchTimer->setSingleShot(true);
    tthSearchInterval = 30000; // start out quick, work up to longer intervals in event func
    TTHSearchTimer->start(tthSearchInterval);

    protocolCapabilityRequestTimer = new QTimer(this);
    connect(protocolCapabilityRequestTimer, SIGNAL(timeout()), this, SLOT(protocolCapabilityRequestTimerEvent()));
    protocolCapabilityRequestTimer->setSingleShot(false);
    protocolCapabilityRequestTimer->setInterval(3000);
}

DownloadTransfer::~DownloadTransfer()
{
    // save bucketFlushStateBitmap to db
    saveBucketStateBitmap();

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

    TTHSearchTimer->deleteLater();
    transferTimer->deleteLater();
    transferRateCalculationTimer->deleteLater();
    protocolCapabilityRequestTimer->deleteLater();
}

// Data packets that are related to our TTH gets dispatched to this entry point.
// Here, we dispatch them to their relevant segments.
// We perform binary lookups on transferSegmentTable on the offset in the datagram header and dispatch accordingly.
void DownloadTransfer::incomingDataPacket(quint8, quint64 offset, QByteArray data)
{
    // If the segment is critically IO bound, we start dropping packets as a last resort
    if (Q_UNLIKELY(bucketHashQueueLength + bucketFlushQueueLength > HASH_BUCKET_QUEUE_CRITICAL_THRESHOLD))
        return;

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

// Send a request to the hash thread to compute the tiger tree hash of a 1MB bucket
void DownloadTransfer::requestHashBucket(QByteArray rootTTH, int bucketNumber, QByteArray *bucket)
{
    if (bucketFlushStateBitmap.at(bucketNumber) == BucketNotFlushed)
    {
        bucketHashQueueLength++;
        congestionTest();
        emit hashBucketRequest(rootTTH, bucketNumber, *bucket);
        // This is to prevent double requests when bucket ends and segment ends coincide.
        // A failed hash check must reset this.
        bucketFlushStateBitmap[bucketNumber] = BucketFlushed;
        transferSegmentStateBitmap[bucketNumber] = SegmentCurrentlyHashing;
    }
}

// The hash thread calls this when it has finished calculating the tiger tree hash of a 1MB bucket
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
            downloadBucketTable->value(bucketNumber)->clear();
            // TODO: emit MISTAKE!
        }
    }
    // TODO: must check that tth tree item was received before requesting bucket hash.
    bucketHashQueueLength--;
    congestionTest();
}

// Our hash tree request's answers are sequentially dispatched to this entry point.
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

    treeUpdatesSinceTimer += iter;

    //int prev = -1;
    //QMapIterator<int, QByteArray*> i(downloadBucketHashLookupTable);
    //while (i.hasNext())
    //{
    //    i.next();
    //    if (i.key() == prev + 1)
    //        prev++;
    //    else
    //        break;
    //}
    // Replacing above to use same method of checking for the highest index, seems like less work.
    // BIG TODO: make sure that downloadBucketHashLookupTable gets populated with data from database if present.

    int lastHashBucketReceived = getLastHashBucketNumberReceived();
    int lastBucket = calculateBucketNumber(fileSize);
    lastBucket = fileSize % HASH_BUCKET_SIZE == 0 ? lastBucket - 1 : lastBucket;

    if (lastHashBucketReceived == hashTreeWindowEnd)
        requestHashTree(lastHashBucketReceived);

    //Start downloading if total hash tree has been downloaded
    else if (lastHashBucketReceived == lastBucket)
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
}

int DownloadTransfer::getTransferType()
{
    return TRANSFER_TYPE_DOWNLOAD;
}

// Restore transfer segment block state bitmaps if the database knows about them
void DownloadTransfer::setBucketFlushStateBitmap(QByteArray bitmap)
{
    if (bucketFlushStateBitmap.length() == 0)
    {
        bucketFlushStateBitmap = bitmap;

        // Important: bucket flush states must coincide with transfer segment states, otherwise we will need to loop-compare-set
        //SegmentNotDownloaded = 0x01,
        //SegmentDownloaded = 0x02,
        //BucketNotFlushed = 0x01,
        //BucketFlushed = 0x02
        transferSegmentStateBitmap = bitmap;
    }
    // If the asynchronous signal arrives late, we must iterate and update, in case things started happening already.
    else if (bitmap.length() == bucketFlushStateBitmap.length())
    {
        for (int i = 0; i < bitmap.length(); i++)
        {
            if (transferSegmentStateBitmap.at(i) == SegmentNotDownloaded)
            {
                bucketFlushStateBitmap[i] = bitmap.at(i);
                transferSegmentStateBitmap[i] = bitmap.at(i);
            }
        }
    }
}

// Initializes transfer segment block state bitmaps and start the transfer timer to get things moving.
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

    transferTimer->start();
    timerBrakes = 0;
}

void DownloadTransfer::pauseTransfer()
{
    status = TRANSFER_STATE_PAUSED;  //TODO
}

void DownloadTransfer::abortTransfer()
{
    status = TRANSFER_STATE_ABORTING;
    // bucketFlushStateBitmap is saved in the destructor, we shouldn't do it here too.
    emit abort(this);
}

// TransferManager sticks peers into a DownloadTransfer via this entry point.
// It calls a request for the peer's protocol abilities, for without it, all hope is lost.
// Not to be confused with newPeer(), which adds the peer and its capabilities to remotePeerInfoTable
void DownloadTransfer::addPeer(QHostAddress peer)
{
    if (peer.toIPv4Address() > 0 && !remotePeerInfoRequestPool.contains(peer))
    {
        remotePeerInfoRequestPool.insert(peer, 1);
        emit requestProtocolCapability(peer, this);
        if (!protocolCapabilityRequestTimer->isActive())
            protocolCapabilityRequestTimer->start();
    }
}

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

// When a 1MB bucket lands from abroad, this is the beginning of its journey to disk.
// We also determine here when the last bucket has landed that the transfer is complete.
void DownloadTransfer::flushBucketToDisk(int &bucketNumber)
{
    // TODO: decide where to store these files
    //QString tempFileName;
    //tempFileName.append(TTHBase32);
    //tempFileName.append(".");
    //tempFileName.append(QString::number(bucketNumber));

    //emit flushBucket(tempFileName, downloadBucketTable->value(bucketNumber));
    //emit assembleOutputFile(TTHBase32, filePathName, bucketNumber, lastBucketNumber);

    QByteArray* bucketPtr = downloadBucketTable->value(bucketNumber);
    bucketFlushQueueLength++;
    congestionTest();
    downloadBucketTable->remove(bucketNumber); // just remove entry, bucket pointer gets deleted in BucketFlushThread
    transferSegmentStateBitmap[bucketNumber] = SegmentCurrentlyFlushing;
    emit flushBucketDirect(filePathName, bucketNumber, bucketPtr, TTH);
}

// This timer event is mainly used to negotiate the transfer of hash trees before the segments are created and the download begins.
// It is also routed to the segments, so that one timer tickles them all.
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
        int lastHashBucketReceived = getLastHashBucketNumberReceived();

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
        else if (treeUpdatesSinceTimer == 0)
        {
            requestHashTree(lastHashBucketReceived, true);
            qDebug() << "Timer request TTH tree " << lastHashBucketReceived + 1;
        }
        treeUpdatesSinceTimer = 0;
    }
}

inline int DownloadTransfer::calculateBucketNumber(quint64 fileOffset)
{
    return (int)(fileOffset >> 20);
}

// This gets called when a segment completes to perform some bookkeeping and allocate resources to download some more if applicable.
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

// This gets called when a segment fails and we need to perform some cleaning duties.
void DownloadTransfer::segmentFailed(TransferSegment *segment)
{
    // remote end dead, segment given up hope. mark everything not downloaded as not downloaded, so that
    // the block allocator can give them to other segments that do work.
    // do not worry if this is the last working segment, if it breaks, it can resume on successful TTH search reply.
    int startBucket = calculateBucketNumber(segment->getSegmentStart());
    int endBucket = calculateBucketNumber(segment->getSegmentEnd());
    for (int i = startBucket; i <= endBucket; i++)
        if (transferSegmentStateBitmap.at(i) == SegmentCurrentlyDownloading)
        {
            transferSegmentStateBitmap[i] = SegmentNotDownloaded;
            if (downloadBucketTable->value(i))
                downloadBucketTable->value(i)->clear();
        }

    transferSegmentTable.remove(segment->getSegmentStart());
    //currentActiveSegments--;
    // currently the object keeps sitting in remotePeerInfoTable and gets destroyed once the download completes.
    // this can be improved (TODO)
    // UPDATE: see if this breaks anything.
    //       : Rational: If a single host shares an object and becomes unavailable mid transfer, the transfer
    //       : stalls indefinitely after the segment fails, even after the host comes back online again.
    QHostAddress h = segment->getSegmentRemotePeer();
    segment->deleteLater();
    
    //Remove offending peer
    //remotePeerInfoTable.remove(h); // TODO: flag, don't remove
    remotePeerInfoTable[h].transferSegment = 0;
    remotePeerInfoTable[h].failureCount++;

    //Update the alternates
    emit searchTTHAlternateSources(TTH);

    // We should not try different peers in the same segment, since they may be of different protocols.
    // When a segment fails, it gets destroyed. Its funeral process should clean up behind it so that there are
    // no more clever pointers trying to find it or transferSegmentTable entries trying to get at it.
    QHostAddress nextPeer = getBestIdlePeer();
    if (nextPeer.isNull())
        currentActiveSegments--;
    else
    {
        TransferSegment *download = createTransferSegment(nextPeer);
        if (download)
        {
            remotePeerInfoTable[nextPeer].transferSegment = download;
            downloadNextAvailableChunk(download);
        }
        else
            currentActiveSegments--;
    }
}

// This is the segment block allocator.
// We ask it for blocks available for download, to give them to a segment that needs work.
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

// When we receive remote peer capability response, we immediately take action.
// If we have room for an extra segment, we instantiate one and put it to work.
// newPeer() updates remotePeerInfoTable
void DownloadTransfer::receivedPeerProtocolCapability(QHostAddress peer, quint8 protocols)
{
    newPeer(peer, protocols);
    remotePeerInfoRequestPool.remove(peer);
    if (remotePeerInfoRequestPool.isEmpty())
        protocolCapabilityRequestTimer->stop();

    if (currentActiveSegments < MAXIMUM_SIMULTANEOUS_SEGMENTS)
    {
        currentActiveSegments++;
        TransferSegment *download = createTransferSegment(peer);
        if (download)
        {
            remotePeerInfoTable[peer].transferSegment = download;
            downloadNextAvailableChunk(download);
        }
    }
}

// Add a peer to remotePeerInfoTable
// Segment pointer is initially null, since it is not instantiated.
void DownloadTransfer::newPeer(QHostAddress peer, quint8 protocols)
{
    if (!remotePeerInfoTable.contains(peer))
    {
        RemotePeerInfoStruct rpis;
        rpis.bytesTransferred = 0;
        rpis.protocolCapability = protocols;
        rpis.transferSegment = 0;
        rpis.failureCount = 0;
        remotePeerInfoTable.insert(peer, rpis);
    }
}

// Create a new transfer segment around QHostAddress(peer)
// This function decides which protocol the segment will use by consulting remotePeerInfoTable
// The segment is primed with a pointer to our buckets (transfer level)
// Remote host, TTH and filesize is set.
// Returns 0 on failure.
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
            if (!download)
                return download;

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

// Instantiate an object of TransferProtocol and connect its signals.
// Returns 0 if specified protocol is not supported.
TransferSegment* DownloadTransfer::newConnectedTransferSegment(TransferProtocol p)
{
    TransferSegment* download = 0;
    switch(p)
    {
    case FailsafeTransferProtocol:
        download = new FSTPTransferSegment(this);
        break;
    case uTPProtocol:
        download = new uTPTransferSegment(this);
        break;
    case BasicTransferProtocol:
    case ArpmanetFECProtocol:
        return download;
        break;
    default:
        return download;
    }
    emit requestNextSegmentId(download);

    connect(download, SIGNAL(hashBucketRequest(QByteArray,int,QByteArray*)), this, SLOT(requestHashBucket(QByteArray,int,QByteArray*)));
    connect(download, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32)),
            this, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32)));
    connect(download, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)), this, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)));
    connect(transferTimer, SIGNAL(timeout()), download, SLOT(transferTimerEvent()));
    connect(download, SIGNAL(requestNextSegment(TransferSegment*)), this, SLOT(segmentCompleted(TransferSegment*)));
    connect(download, SIGNAL(transferRequestFailed(TransferSegment*)), this, SLOT(segmentFailed(TransferSegment*)));
    return download;
}

// Get next chunk from block allocator
// Update incoming packet dispatch segment map
// Call startDownloading() in segment
void DownloadTransfer::downloadNextAvailableChunk(TransferSegment *download, int length)
{
    TransferSegmentTableStruct t;
    SegmentOffsetLengthStruct s = getSegmentForDownloading(length);
    qint64 segmentStart = (qint64)s.segmentBucketOffset * HASH_BUCKET_SIZE;
    Q_ASSERT(segmentStart >= 0);
    t.segmentEnd = (qint64)(s.segmentBucketOffset + s.segmentBucketCount) * HASH_BUCKET_SIZE;
    t.segmentEnd = t.segmentEnd > fileSize ? fileSize : t.segmentEnd;
    Q_ASSERT(t.segmentEnd >= 0);
    
    download->setSegmentStart(segmentStart);
    download->setSegmentEnd(t.segmentEnd);
    if (segmentStart != t.segmentEnd) // otherwise done
    {
        t.transferSegment = download;
        transferSegmentTable.insert(segmentStart, t);
        download->startDownloading();
    }
    else if (remotePeerInfoTable.contains(download->getSegmentRemotePeer()))
    {
        download->deleteLater();
        remotePeerInfoTable[download->getSegmentRemotePeer()].transferSegment = 0;
        currentActiveSegments--;
    }
}

void DownloadTransfer::TTHSearchTimerEvent()
{
    if (currentActiveSegments < MAXIMUM_SIMULTANEOUS_SEGMENTS)
        emit searchTTHAlternateSources(TTH);
    if (tthSearchInterval < 300000)  // 5 min max interval
        tthSearchInterval += tthSearchInterval;
    TTHSearchTimer->start(tthSearchInterval);

    // borrow this timer to save the bucket flush state periodically.
    saveBucketStateBitmap();
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

QByteArray DownloadTransfer::getTransferStateBitmap()
{
    return transferSegmentStateBitmap;
}

int DownloadTransfer::getLastHashBucketNumberReceived()
{
    int lastHashBucketReceived = -1;
    if (!downloadBucketHashLookupTable.isEmpty())
    {
        QMap<int, QByteArray*>::const_iterator i = downloadBucketHashLookupTable.constEnd();
        --i;
        lastHashBucketReceived = i.key();
    }
    return lastHashBucketReceived;
}

int DownloadTransfer::getSegmentCount()
{
    return currentActiveSegments;
}

SegmentStatusStruct DownloadTransfer::getSegmentStatuses()
{
    SegmentStatusStruct s = {0,0,0,0,0};
        
    //Iterate through segments and test statuses
    QMapIterator<quint64, TransferSegmentTableStruct> i(transferSegmentTable);
    while (i.hasNext())
    {
        i.next();
        int status = i.value().transferSegment->getSegmentStatus();
        switch (status)
        {
        case TRANSFER_STATE_RUNNING:
            s.running++;
            break;
        case TRANSFER_STATE_FINISHED:
            s.finished++;
            break;
        case TRANSFER_STATE_STALLED:
            s.stalled++;
            break;
        case TRANSFER_STATE_INITIALIZING:
            s.initializing++;
            break;
        case TRANSFER_STATE_FAILED:
            s.failed++;
            break;
        }
    }

    return s;
}

void DownloadTransfer::incomingTransferError(quint64 offset, quint8 error)
{
    TransferSegment *t =0;
    QMap<quint64, TransferSegmentTableStruct>::const_iterator i = transferSegmentTable.upperBound(offset);
    if (Q_UNLIKELY(i == transferSegmentTable.constEnd()))
        --i;
    if (Q_UNLIKELY(i.key() <= offset && i.value().segmentEnd >= offset))
        t = i.value().transferSegment;
    else if (Q_LIKELY(i != transferSegmentTable.constBegin()))
    {
        --i;
        if (Q_LIKELY(i.key() <= offset && i.value().segmentEnd >= offset))
            t = i.value().transferSegment;
    }
    if (t)
        segmentFailed(t);
}

void DownloadTransfer::bucketFlushed(int bucketNo)
{
    bucketFlushQueueLength--;
    congestionTest();
    transferSegmentStateBitmap[bucketNo] = SegmentDownloaded;

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
        emit renameIncompleteFile(filePathName); // TODO: revisit this when fixing resumable downloads
        emit transferFinished(TTH);

        // let's rather keep the state finished, just emit the abort signal, so that the destructor knows not to persist the block bitmap on finish.
        //abortTransfer();
        emit abort(this);
    }
}

void DownloadTransfer::bucketFlushFailed(int bucketNo)
{
    bucketFlushQueueLength--;
    congestionTest();
    transferSegmentStateBitmap[bucketNo] = SegmentNotDownloaded;
    bucketFlushStateBitmap[bucketNo] = BucketNotFlushed;
    // TODO: break the possible infinite download loop by stopping the transfer at some point.
}

void DownloadTransfer::congestionTest()
{
    qDebug() << "DownloadTransfer::congestionTest(): hash queue : bucket queue " << bucketHashQueueLength << bucketFlushQueueLength;
    QHashIterator<QHostAddress, RemotePeerInfoStruct> i(remotePeerInfoTable);
    if (!iowait && (bucketFlushQueueLength + bucketHashQueueLength > HASH_BUCKET_QUEUE_CONGESTION_THRESHOLD))
    {
        iowait = true;
        status = TRANSFER_STATE_IOWAIT;
        transferTimer->stop();
        while (i.hasNext())
        {
            TransferSegment *s = i.next().value().transferSegment;
            if (s)
                s->pauseDownload();
        }
    }
    else if (iowait && (bucketFlushQueueLength + bucketHashQueueLength <= HASH_BUCKET_QUEUE_CONGESTION_THRESHOLD / 4))
    {
        iowait = false;
        status = TRANSFER_STATE_RUNNING;
        transferTimer->start();
        while (i.hasNext())
        {
            TransferSegment *s = i.next().value().transferSegment;
            if (s)
                s->unpauseDownload();
        }
    }
}

void DownloadTransfer::requestHashTree(int lastHashBucketReceived, bool timerRequest)
{
    if (!remotePeerInfoTable.isEmpty() && timerRequest)
    {
        int n = (int)((double)qrand() / RAND_MAX * remotePeerInfoTable.size());
        QHashIterator<QHostAddress, RemotePeerInfoStruct> it(remotePeerInfoTable);
        for (int i = 0; i <= n; i++)
            if (it.hasNext())
                treeRequestHost = it.next().key();
    }
    if (!treeRequestHost.isNull())
    {
        emit TTHTreeRequest(treeRequestHost, TTH, lastHashBucketReceived + 1, HASH_TREE_WINDOW_LENGTH);
        hashTreeWindowEnd = lastHashBucketReceived + HASH_TREE_WINDOW_LENGTH;
        qDebug() << "Request TTH tree " << lastHashBucketReceived + 1 << calculateBucketNumber(fileSize);
    }
}

void DownloadTransfer::protocolCapabilityRequestTimerEvent()
{
    QMutableHashIterator<QHostAddress, int> i(remotePeerInfoRequestPool);
    while(i.hasNext())
    {
        emit requestProtocolCapability(i.peekNext().key(), this);
        i.next().value()++;
        if (i.peekPrevious().value() <= 5)
            i.remove();
    }
    if (remotePeerInfoRequestPool.isEmpty())
        protocolCapabilityRequestTimer->stop();
}

QHostAddress DownloadTransfer::getBestIdlePeer()
{
    QHostAddress bestPeer = QHostAddress();
    int leastFails = 1000;
    QHashIterator<QHostAddress, RemotePeerInfoStruct> i(remotePeerInfoTable);
    while (i.hasNext())
    {
        QHostAddress h = i.peekNext().key();
        RemotePeerInfoStruct s = i.next().value();
        if ((s.transferSegment == 0) && (s.failureCount < leastFails))
        {
            leastFails = s.failureCount;
            if (leastFails == 0)
                return h;

            bestPeer = h;
        }
    }
    return bestPeer;
}

void DownloadTransfer::saveBucketStateBitmap()
{
    r.clear();
    for (int i = 0; i < transferSegmentStateBitmap.length(); i++)
    {
        if (transferSegmentStateBitmap.at(i) == SegmentDownloaded)
            r[i] = BucketFlushed;
        else
            r[i] = BucketNotFlushed;
    }
    emit saveBucketFlushStateBitmap(TTH, r);
}
