#include "downloadtransfer.h"
#include <QThread>

DownloadTransfer::DownloadTransfer(QObject *parent) : Transfer(parent)
{
    downloadBucketTable = new QHash<int, QByteArray *>();
    transferRate = 0;
    transferProgress = 0;
    bytesWrittenSinceUpdate = 0;
    bytesWrittenSinceCalculation = 0;
    initializationStateTimerBrakes = 0;
    status = TRANSFER_STATE_INITIALIZING;
    remoteHost = QHostAddress("0.0.0.0");
    treeRequestHost = QHostAddress();
    //currentActiveSegments = 0;
    hashTreeWindowEnd = 0;
    bucketHashQueueLength = 0;
    bucketFlushQueueLength = 0;
    iowait = false;
    treeUpdatesSinceTimer = 0;
    zeroSegmentTimeoutCount = 0;

    transferRateCalculationTimer = new QTimer(this);
    //Rather use the frequency of updates from the GUI, otherwise bytes might be missing and it would be unreliable to determine total bytes sent/received
    // OK, that is fine, but this timer switches a transfer into stalled mode when nothing is going on.
    // If we want to spare a timer, we can kick this slot from the GUI's timer as well.
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

    newSegmentTimer = new QTimer(this);
    connect(newSegmentTimer, SIGNAL(timeout()), this, SLOT(newSegmentTimerEvent()));
    newSegmentTimer->setSingleShot(false);
    newSegmentTimer->start(10000);
}

DownloadTransfer::~DownloadTransfer()
{
    // save bucketFlushStateBitmap to db
    saveBucketStateBitmap();

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
    {
        r.next();
        if (r.value().transferSegment)
        {
            emit removeTransferSegmentPointer(r.value().transferSegment->getSegmentId());
            //r.value().transferSegment->deleteLater();
            qDebug() << "DownloadTransfer Destructor: " << r.value().transferSegment;
        }
    }
    
    TTHSearchTimer->deleteLater();
    transferTimer->deleteLater();
    transferRateCalculationTimer->deleteLater();
    protocolCapabilityRequestTimer->deleteLater();
    newSegmentTimer->deleteLater();
}

// Data packets that are related to our TTH gets dispatched to this entry point.
// Here, we dispatch them to their relevant segments.
// We perform binary lookups on transferSegmentTable on the offset in the datagram header and dispatch accordingly.
void DownloadTransfer::incomingDataPacket(quint8, qint64 offset, QByteArray data)
{
    // If the segment is critically IO bound, we start dropping packets as a last resort
    if (Q_UNLIKELY(bucketHashQueueLength + bucketFlushQueueLength > HASH_BUCKET_QUEUE_CRITICAL_THRESHOLD))
        return;

    // The iteration code below does not expect an empty table.
    if (transferSegmentTable.isEmpty())
        return;

    // map are sorted in key ascending order
    // QMap::lowerBound will most likely find the segment just before the one we are interested in
    QMap<qint64, TransferSegmentTableStruct>::const_iterator i = transferSegmentTable.upperBound(offset);
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
    // segment updates this, direct dispatch bypasses this function
    //bytesWrittenSinceUpdate += data.size();
}

// Send a request to the hash thread to compute the tiger tree hash of a 1MB bucket
void DownloadTransfer::requestHashBucket(QByteArray rootTTH, int bucketNumber, QByteArray *bucket, QHostAddress peer)
{
    if (bucketFlushStateBitmap.at(bucketNumber) == BucketNotFlushed)
    {
        bucketHashQueueLength++;
        congestionTest();
        emit hashBucketRequest(rootTTH, bucketNumber, *bucket, peer);
        // This is to prevent double requests when bucket ends and segment ends coincide.
        // A failed hash check must reset this.
        bucketFlushStateBitmap[bucketNumber] = BucketFlushed;
        transferSegmentStateBitmap[bucketNumber] = SegmentCurrentlyHashing;
    }
}

// The hash thread calls this when it has finished calculating the tiger tree hash of a 1MB bucket
void DownloadTransfer::hashBucketReply(int bucketNumber, QByteArray bucketTTH, QHostAddress peer)
{
    if (downloadBucketHashLookupTable.contains(bucketNumber))
    {
        if (*downloadBucketHashLookupTable.value(bucketNumber) == bucketTTH)
        {
            flushBucketToDisk(bucketNumber);
            //qDebug() << "DownloadTransfer::hashBucketReply() checksum matched, flushing bucket" << bucketNumber;
        }
        else
        {
            transferSegmentStateBitmap[bucketNumber] = SegmentNotDownloaded;
            bucketFlushStateBitmap[bucketNumber] = BucketNotFlushed;
            if (downloadBucketTable->value(bucketNumber))
                downloadBucketTable->value(bucketNumber)->clear();
            qDebug() << "DownloadTransfer::hashBucketReply() checksum mismatch" << bucketNumber;
            remotePeerInfoTable[peer].checksumMismatchCount++;
            if (remotePeerInfoTable.value(peer).checksumMismatchCount >= 10)
            {
                remotePeerInfoTable[peer].blacklisted = true;
                if (remotePeerInfoTable.value(peer).transferSegment)
                    segmentFailed(remotePeerInfoTable.value(peer).transferSegment);
            }
        }
    }
    else
    {
        transferSegmentStateBitmap[bucketNumber] = SegmentNotDownloaded;
        bucketFlushStateBitmap[bucketNumber] = BucketNotFlushed;
        if (downloadBucketTable->value(bucketNumber))
            downloadBucketTable->value(bucketNumber)->clear();
        qDebug() << "DownloadTransfer::hashBucketReply() bucket hashed not in hash tree" << bucketNumber;
        emit requeue(this, true);
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
    // if file does not exist (.incomplete file moved or deleted), do not restore bitmap
    // setFileName() should have been called by now, otherwise fileExists == false.
    if (!fileExists)
    {
        qDebug() << "DownloadTransfer::setBucketFlushStateBitmap(): file exists == false, aborting bitmap load.";
        return;
    }

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
void DownloadTransfer::addPeer(QHostAddress peer, QByteArray cid)
{
    if (peer.toIPv4Address() > 0 && !remotePeerInfoRequestPool.contains(peer) && !remotePeerInfoTable.contains(peer))
    {
        RemotePeerInfoRequestPoolStruct s;
        s.retries = 1;
        s.cid = cid;
        remotePeerInfoRequestPool.insert(peer, s);
        emit requestProtocolCapability(peer, this);
        if (!protocolCapabilityRequestTimer->isActive())
            protocolCapabilityRequestTimer->start();
    }
}

void DownloadTransfer::transferRateCalculation()
{
    if ((status == TRANSFER_STATE_RUNNING) && (bytesWrittenSinceCalculation == 0))
        status = TRANSFER_STATE_STALLED;
    else if ((status == TRANSFER_STATE_STALLED) && (bytesWrittenSinceCalculation > 0))
        status = TRANSFER_STATE_RUNNING;

    bytesWrittenSinceCalculation = 0;
    // snapshot the transfer rate as the amount of bytes written in the last second
    //transferRate = bytesWrittenSinceUpdate;
    //bytesWrittenSinceUpdate = 0;
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
void DownloadTransfer::segmentFailed(TransferSegment *segment, quint8 error, bool startIdleSegment)
{
    qDebug() << "DownloadTransfer::segmentFailed() " << (void *)segment;
    qDebug() << "DownloadTransfer::segmentFailed() peer segmentid error" << segment->getSegmentRemotePeer() << segment->getSegmentId() << error;
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
    // currently the object keeps sitting in remotePeerInfoTable and gets destroyed once the download completes.
    // this can be improved (TODO)
    // UPDATE: see if this breaks anything.
    //       : Rational: If a single host shares an object and becomes unavailable mid transfer, the transfer
    //       : stalls indefinitely after the segment fails, even after the host comes back online again.
    QHostAddress h = segment->getSegmentRemotePeer();
    QString s = h.toString();
    //Remove offending peer

    //Check if removed correctly
    int count = 0;
    QMutableHashIterator<QHostAddress, RemotePeerInfoStruct> mi(remotePeerInfoTable);
    while (mi.hasNext())
    {
        mi.next();
        if (mi.value().transferSegment == segment)
        {
            mi.value().transferSegment = 0;
            count++;
        }
    }
    
    qDebug() << "DownloadTransfer::segmentFailed() " << segment << remotePeerInfoTable[h].transferSegment << h;
    if (error & (FileIOError | InvalidOffsetError | FileNotSharedError))
        remotePeerInfoTable[h].blacklisted = true;

    remotePeerInfoTable[h].failureCount++;
    remotePeerInfoTable[h].bytesTransferred += segment->getBytesTransferred();
    emit unflagDownloadPeer(h);

    quint32 segmentId = segment->getSegmentId();
    emit removeTransferSegmentPointer(segmentId);

    segment->deleteLater();
    //currentActiveSegments--;

    //Update the alternates
    emit searchTTHAlternateSources(TTH);

    // We should not try different peers in the same segment, since they may be of different protocols.
    // When a segment fails, it gets destroyed. Its funeral process should clean up behind it so that there are
    // no more clever pointers trying to find it or transferSegmentTable entries trying to get at it.
    if (startIdleSegment)
    {
        QHostAddress nextPeer = getBestIdlePeer();
        if (!nextPeer.isNull())
        {
            TransferSegment *download = createTransferSegment(nextPeer);
            if (download)
            {
                //currentActiveSegments++;
                //remotePeerInfoTable[nextPeer].transferSegment = download;
                downloadNextAvailableChunk(download);
            }
        }
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
    if (remotePeerInfoRequestPool.contains(peer))
        newPeer(peer, protocols, remotePeerInfoRequestPool.value(peer).cid);

    remotePeerInfoRequestPool.remove(peer);
    //qDebug() << "DownloadTransfer::receivedPeerProtocolCapability() peer protocols" << peer << protocols;
    if (remotePeerInfoRequestPool.isEmpty())
        protocolCapabilityRequestTimer->stop();

    if (currentActiveSegments() < MAXIMUM_SIMULTANEOUS_SEGMENTS)
    {
        // Do not necessarily use this peer for the next segment, rather ask getBestIdlePeer() in case this one is already busy in another segment.
        QHostAddress nextPeer = getBestIdlePeer();
        TransferSegment *download = 0;
        if (!nextPeer.isNull())
            download = createTransferSegment(nextPeer);

        if (download)
        {
            //currentActiveSegments++;
            //remotePeerInfoTable[peer].transferSegment = download;
            downloadNextAvailableChunk(download);
        }
    }
}

// Add a peer to remotePeerInfoTable
// Segment pointer is initially null, since it is not instantiated.
void DownloadTransfer::newPeer(QHostAddress peer, quint8 protocols, QByteArray cid)
{
    if (!remotePeerInfoTable.contains(peer))
    {
        RemotePeerInfoStruct rpis;
        rpis.bytesTransferred = 0;
        rpis.protocolCapability = protocols;
        rpis.transferSegment = 0;
        rpis.failureCount = 0;
        rpis.blacklisted = false;
        rpis.lastStartTime = 0;
        rpis.lastBytesQueued = 0;
        rpis.lastTransferRate = 0;
        rpis.checksumMismatchCount = 0;
        rpis.cid = cid;
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
            if (isNonDispatchedProtocol(p))
                remotePeerInfoTable[peer].triedProtocols.append(p);
            download  = newConnectedTransferSegment(p);
            if (!download)
                return download;

            remotePeerInfoTable[peer].transferSegment = download;
            download->setDownloadBucketTablePointer(downloadBucketTable);
            download->setRemoteHost(peer);
            download->setTTH(TTH);
            download->setFileSize(fileSize);
            download->setRemoteCID(remotePeerInfoTable.value(peer).cid);
            break;
        }
    }
    emit flagDownloadPeer(peer);
    qDebug() << "DownloadTransfer::createTransferSegment()" << download->getSegmentId() << download;
    return download;
}

// Instantiate an object of TransferProtocol and connect its signals.
// Returns 0 if specified protocol is not supported.
TransferSegment* DownloadTransfer::newConnectedTransferSegment(TransferProtocol p)
{
    QPointer<TransferSegment> download = 0;
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
    emit setTransferSegmentPointer(download->getSegmentId(), download);

    connect(download, SIGNAL(hashBucketRequest(QByteArray,int,QByteArray*,QHostAddress)), this, SLOT(requestHashBucket(QByteArray,int,QByteArray*,QHostAddress)));
    connect(download, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32,QByteArray)),
            this, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32,QByteArray)));
    connect(download, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)), this, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)));
    connect(transferTimer, SIGNAL(timeout()), download, SLOT(transferTimerEvent()));
    connect(download, SIGNAL(requestNextSegment(TransferSegment*)), this, SLOT(segmentCompleted(TransferSegment*)));
    connect(download, SIGNAL(transferRequestFailed(TransferSegment*,quint8,bool)), this, SLOT(segmentFailed(TransferSegment*,quint8,bool)));
    connect(download, SIGNAL(removeTransferSegmentPointer(quint32)), this, SIGNAL(removeTransferSegmentPointer(quint32)));
    connect(download, SIGNAL(updateDirectBytesStats(int)), this, SLOT(updateDirectBytesStats(int)));
    return download;
}

// Get next chunk from block allocator
// Update incoming packet dispatch segment map
// Call startDownloading() in segment
void DownloadTransfer::downloadNextAvailableChunk(TransferSegment *download, int length, int recursionLimit)
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
    QHostAddress h = download->getSegmentRemotePeer();

    if (segmentStart != t.segmentEnd) // otherwise done
    {
        // update indirect dispatch routing table
        t.transferSegment = download;
        transferSegmentTable.insert(segmentStart, t);

        // calculate survival of the fittest stats
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        int delta = currentTime - remotePeerInfoTable.value(h).lastStartTime;
        remotePeerInfoTable[h].lastTransferRate = remotePeerInfoTable.value(h).lastBytesQueued / delta;
        remotePeerInfoTable[h].lastStartTime = currentTime;
        remotePeerInfoTable[h].lastBytesQueued = t.segmentEnd - segmentStart;

        // commence transfer
        download->startDownloading();
    }
    else if (recursionLimit > 0 && remotePeerInfoTable.contains(download->getSegmentRemotePeer()))
    {
        // first we identify a slow segment for hostile takeover
        TransferSegment *slowSegment = getSlowestActivePeer();
        if (slowSegment && slowSegment != download)
        {
            qDebug() << "DownloadTransfer::downloadNextAvailableChunk() kick slow segment" << slowSegment->getSegmentId() << slowSegment;
            segmentFailed(slowSegment, SlowSegmentHostileTakeover, false);
            downloadNextAvailableChunk(download, 1, --recursionLimit);
        }
        else // if unsuccessful, we are done here.
        {
            QMutableHashIterator<QHostAddress, RemotePeerInfoStruct> mi(remotePeerInfoTable);
            int count = 0;
            while (mi.hasNext())
            {
                mi.next();
                if (mi.value().transferSegment == download)
                {
                    mi.value().transferSegment = 0;
                    count++;
                }
            }
            //remotePeerInfoTable[h].transferSegment = 0;

            qDebug() << "DownloadTransfer::downloadNextAvailableChunk() no more blocks to download, destroying:" << download << remotePeerInfoTable[h].transferSegment << h;
            remotePeerInfoTable[h].bytesTransferred = download->getBytesTransferred();
            emit unflagDownloadPeer(h);
            quint32 segmentId = download->getSegmentId();
            emit removeTransferSegmentPointer(segmentId);
            download->deleteLater();
            //delete download;
            //currentActiveSegments--;
        }
    }
    else if (recursionLimit > 0)
    {
        qDebug() << "DownloadTransfer::downloadNextAvailableChunk(): else reached that should not be reached." << currentActiveSegments();
    }
}

void DownloadTransfer::TTHSearchTimerEvent()
{
    if (currentActiveSegments() < MAXIMUM_SIMULTANEOUS_SEGMENTS)
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
    int segmentsDone = getSegmentsDone();

    //Clause to only use byte precision for files smaller than 100 MB
    if (fileSize >= 100*(1<<20))
        return (int)(((double)segmentsDone / (calculateBucketNumber(fileSize) + 1)) * 100);

    //===== 1 byte precision progress =====
    qint64 dataReceivedNotFlushed = 0;
    int segmentsActive = 0;

    //Go through all active segments and check the amount of data received
    //UPDATE: actually, we only need to iterate downloadBucketTable - otherwise every segment ends up doing it and counting n * bytesReceivedNotFlushed.
    foreach (QByteArray *data, *downloadBucketTable)
        dataReceivedNotFlushed += data->size();

    // Count data inside segments not yet tipped into buckets. (FECTP et al)
    foreach (TransferSegmentTableStruct t, transferSegmentTable)
    {
        if (t.transferSegment)
        {
            dataReceivedNotFlushed += t.transferSegment->getBytesReceivedNotFlushed();
            segmentsActive++;
        }
    }
            
    qint64 bytesDone = segmentsDone * HASH_BUCKET_SIZE + dataReceivedNotFlushed;
    if (fileSize == 0)
        return 0;

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
    return currentActiveSegments();
}

SegmentStatusStruct DownloadTransfer::getSegmentStatuses()
{
    SegmentStatusStruct s = {0,0,0,0,0};
        
    //Iterate through segments and test statuses
    QMapIterator<qint64, TransferSegmentTableStruct> i(transferSegmentTable);
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

void DownloadTransfer::incomingTransferError(qint64 offset, quint8 error)
{
    if (transferSegmentTable.isEmpty())
        return;

    TransferSegment *t =0;
    QMap<qint64, TransferSegmentTableStruct>::const_iterator i = transferSegmentTable.upperBound(offset);
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
        segmentFailed(t, error);
}

void DownloadTransfer::bucketFlushed(int bucketNo)
{
    bucketFlushQueueLength--;
    congestionTest();
    transferSegmentStateBitmap[bucketNo] = SegmentDownloaded;

    int segmentsDone = getSegmentsDone();
    int fileBuckets = getTotalFileSegments();
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
    //qDebug() << "DownloadTransfer::congestionTest(): hash queue : bucket queue " << bucketHashQueueLength << bucketFlushQueueLength;
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
    else
    {
        qDebug() << "Request TTH tree: null request host, not sent.";
    }
}

void DownloadTransfer::protocolCapabilityRequestTimerEvent()
{
    QMutableHashIterator<QHostAddress, RemotePeerInfoRequestPoolStruct> i(remotePeerInfoRequestPool);
    while(i.hasNext())
    {
        emit requestProtocolCapability(i.peekNext().key(), this);
        //qDebug() << "DownloadTransfer::protocolCapabilityRequestTimerEvent() request protocol capability" << i.peekNext().key(), i.peekNext().value();
        i.next().value().retries++;
        if (i.peekPrevious().value().retries <= 5)
            i.remove();
    }
    if (remotePeerInfoRequestPool.isEmpty())
        protocolCapabilityRequestTimer->stop();
}

QHostAddress DownloadTransfer::getBestIdlePeer()
{
    QHostAddress bestPeer = QHostAddress();
    double bestWeight = -1000;
    QHashIterator<QHostAddress, RemotePeerInfoStruct> i(remotePeerInfoTable);
    while (i.hasNext())
    {
        QHostAddress h = i.peekNext().key();
        RemotePeerInfoStruct s = i.next().value();
        if ((s.transferSegment == 0) && (!s.blacklisted))
        {
            double weight = log10((double)s.bytesTransferred + 1) - s.failureCount;
            if (weight > bestWeight)
            {
                if (!pCurrentDownloadingPeers->contains(h))
                {
                    bestWeight = weight;
                    bestPeer = h;
                }
            }
        }
    }

    return bestPeer;
}

TransferSegment* DownloadTransfer::getSlowestActivePeer()
{
    TransferSegment *t = 0;
    qint64 worstTransferRate = LLONG_MAX;
    QHashIterator<QHostAddress, RemotePeerInfoStruct> i(remotePeerInfoTable);
    while (i.hasNext())
    {
        const RemotePeerInfoStruct *s = &i.next().value();
        if ((s->lastTransferRate < worstTransferRate) && s->transferSegment)
        {
            worstTransferRate = s->lastTransferRate;
            t = s->transferSegment;
        }
    }
    return t;
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

void DownloadTransfer::updateDirectBytesStats(int bytes)
{
    bytesWrittenSinceUpdate += bytes;
    bytesWrittenSinceCalculation += bytes;
}

void DownloadTransfer::newSegmentTimerEvent()
{
    if (currentActiveSegments() == 0)
        zeroSegmentTimeoutCount++;
    else
        zeroSegmentTimeoutCount = 0;

    if (zeroSegmentTimeoutCount >= 6)
        //emit abort(this);  // fail the download without removing it from queue.
        emit requeue(this);

    if ((currentActiveSegments() >= MAXIMUM_SIMULTANEOUS_SEGMENTS) || (!(status & (TRANSFER_STATE_RUNNING | TRANSFER_STATE_STALLED))))
        return;

    // Do not try and create new segments when they will be destroyed directly afterwards!
    // I believe ping-ponging segment creation can cause the segfaulting that harasses us.
    if (getUnallocatedBlockCount() == 0)
        return;

    QHostAddress nextPeer = getBestIdlePeer();
    if (!nextPeer.isNull())
    {
        TransferSegment *download = createTransferSegment(nextPeer);
        if (download)
        {
            //remotePeerInfoTable[nextPeer].transferSegment = download;
            //currentActiveSegments++;
            downloadNextAvailableChunk(download, 1);
        }
        else
            remotePeerInfoTable[nextPeer].transferSegment = 0;
    }
}

bool DownloadTransfer::isNonDispatchedProtocol(TransferProtocol protocol)
{
    // If we ever add a transfer protocol that does not run over DispatchIP:DispatchPort/udp, this function must return true for it, so that protocol negotiation can permanently fail for
    // it in case its path is blocked between two peers.
    return false;
}

int DownloadTransfer::getSegmentsDone()
{
    int segmentsDone = 0;
    for (int i = 0; i < transferSegmentStateBitmap.length(); i++)
    {
        if (transferSegmentStateBitmap.at(i) == SegmentDownloaded)
            segmentsDone++;
    }
    return segmentsDone;
}

int DownloadTransfer::getUnallocatedBlockCount()
{
    int unallocatedBlocks = 0;
    for (int i = 0; i < transferSegmentStateBitmap.length(); i++)
    {
        if (transferSegmentStateBitmap.at(i) == SegmentNotDownloaded)
            unallocatedBlocks++;
    }
    return unallocatedBlocks;
}

int DownloadTransfer::getTotalFileSegments()
{
    int fileBuckets = calculateBucketNumber(fileSize);
    fileBuckets = fileSize % HASH_BUCKET_SIZE == 0 ? fileBuckets : fileBuckets + 1;
    return fileBuckets;
}

qint64 DownloadTransfer::getTransferRate()
{
    transferRate = bytesWrittenSinceUpdate;
    bytesWrittenSinceUpdate = 0;

    return transferRate;
}

int DownloadTransfer::currentActiveSegments()
{
    return transferSegmentTable.count();
}
