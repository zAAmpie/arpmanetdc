#include "transfermanager.h"

TransferManager::TransferManager(QHash<QString, QString> *settings, QObject *parent) :
    QObject(parent)
{
    pSettings = settings;
    currentDownloadCount = 0;
    currentUploadCount = 0;
    zeroHostAddress = QHostAddress("0.0.0.0");
    nextSegmentId = qrand();
    if (nextSegmentId == 0)
        nextSegmentId++;
}

TransferManager::~TransferManager()
{
    QHashIterator<QByteArray, Transfer*> it(transferObjectTable);
    while (it.hasNext())
    {
        Transfer *p = it.next().value();
        destroyTransferObject(p);
    }

    QMapIterator<int, QList<DownloadTransferQueueItem>*> i(downloadTransferQueue);
    while (i.hasNext())
        delete i.next().value();
}

// remove the pointer to the transfer object from the transfer object table before deleting the object.
void TransferManager::destroyTransferObject(Transfer* transferObject)
{
    int type = transferObject->getTransferType();

    if (transferObjectTable.remove(*transferObject->getTTH(), transferObject) != 0)
    {
        if (type == TRANSFER_TYPE_DOWNLOAD)
            currentDownloadCount--;
        else if (type == TRANSFER_TYPE_UPLOAD)
            currentUploadCount--;

        QMutableHashIterator<QHostAddress, Transfer *> i(peerProtocolDiscoveryWaitingPool);
        while (i.hasNext())
        {
            i.next();
            if (i.value() == transferObject)
                i.remove();
        }
            
    }

    transferObject->deleteLater();
}

// incoming data packets
void TransferManager::incomingDataPacket(quint8 transferPacket, QHostAddress fromHost, QByteArray datagram)
{
    QByteArray tmp = datagram.mid(2, 8);
    quint64 offset = getQuint64FromByteArray(&tmp);
    QByteArray tth = datagram.mid(10, 24);
    QByteArray data = datagram.mid(34);
    if ((data.length() == datagram.length() - 34) && (transferObjectTable.contains(tth)))
    {
        Transfer *t = getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD);
        if (t)
            t->incomingDataPacket(transferPacket, offset, data);
        else
        {
            t = getTransferObjectPointer(tth, TRANSFER_TYPE_UPLOAD, &fromHost);
            if (t)
            {
                qDebug() << "TransferManager::incomingDataPacket() for upload" << t->getFileName();
                t->incomingDataPacket(transferPacket, offset, data);
            }
        }
    }
}

// incoming direct dispatched data packets
void TransferManager::incomingDirectDataPacket(quint32 segmentId, quint64 offset, QByteArray data)
{
    TransferSegment *t = getTransferSegmentPointer(segmentId);
    if (t)
        t->incomingDataPacket(offset, data);
}

void TransferManager::incomingTransferError(QHostAddress fromHost, QByteArray tth, quint64 offset, quint8 error)
{
    Transfer *t = getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->incomingTransferError(offset, error);
}

// incoming requests for files we share
void TransferManager::incomingUploadRequest(quint8 protocol, QHostAddress fromHost, QByteArray tth, qint64 offset, qint64 length, quint32 segmentId)
{
    qDebug() << "TransferManager::incomingUploadRequest(): Data request offset " << offset << " length " << length;
    Transfer *t = getTransferObjectPointer(tth, TRANSFER_TYPE_UPLOAD, &fromHost);
    if (t)
    {
        t->setFileOffset(offset);
        t->setSegmentLength(length);
        t->startTransfer();
    }
    else
    {
        //Only queue the item if there are "slots" open
        if (currentUploadCount < maximumSimultaneousUploads)
        {
            currentUploadCount++;
            UploadTransferQueueItem *i = new UploadTransferQueueItem;
            i->protocol = protocol;
            i->requestingHost = fromHost;
            i->fileOffset = offset;
            i->requestLength = length;
            i->segmentId = segmentId;
            uploadTransferQueue.insertMulti(tth, i);
            emit filePathNameRequest(tth);
        }
    }
}

// sharing engine replies with file name for tth being requested for download
// this structure assumes only one user requests a specific file during the time it takes to dispatch the request to a transfer object.
// should more than one user try simultaneously, their requests will be deleted off the queue in .remove(tth) and they should try again. oops.
void TransferManager::filePathNameReply(QByteArray tth, QString filename, quint64 fileSize)
{
    if (filename == "" || !uploadTransferQueue.contains(tth))
    {
        uploadTransferQueue.remove(tth);
        return; // TODO: stuur error terug na requesting host
    }
    Transfer *t = new UploadTransfer(this);
    TransferSegment *s = t->createUploadObject(uploadTransferQueue.value(tth)->protocol, uploadTransferQueue.value(tth)->segmentId);
    setTransferSegmentPointer(uploadTransferQueue.value(tth)->segmentId, s);
    connect(t, SIGNAL(abort(Transfer*)), this, SLOT(destroyTransferObject(Transfer*)));
    connect(t, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)), this, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)));
    connect(t, SIGNAL(sendTransferError(QHostAddress,quint8,QByteArray,quint64)), this, SIGNAL(sendTransferError(QHostAddress,quint8,QByteArray,quint64)));
    t->setFileName(filename);
    t->setFileSize(fileSize);
    t->setTTH(tth);
    t->setFileOffset(uploadTransferQueue.value(tth)->fileOffset);
    t->setSegmentLength(uploadTransferQueue.value(tth)->requestLength);
    t->setRemoteHost(uploadTransferQueue.value(tth)->requestingHost);
    uploadTransferQueue.remove(tth);
    transferObjectTable.insertMulti(tth, t);
    t->startTransfer();
}

// incoming requests from user interface for files we want to download
// the higher the priority, the lower the number.
void TransferManager::queueDownload(int priority, QByteArray tth, QString filePathName, quint64 fileSize, QHostAddress fileHost)
{
    if (!downloadTransferQueue.contains(priority))
    {
        QList<DownloadTransferQueueItem> *list = new QList<DownloadTransferQueueItem>();
        downloadTransferQueue.insert(priority, list);
    }

    //Test if item isn't already in the queue
    bool found = false;
    int size = downloadTransferQueue.value(priority)->size();
    foreach (DownloadTransferQueueItem item, *downloadTransferQueue.value(priority))
    {
        if (item.tth == tth)
        {
            found = true;
            break;
        }
    }

    //Only queue the item if it's not already in the queue and not busy downloading
    if (!found && !getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD))
    {
        DownloadTransferQueueItem i;
        i.filePathName = filePathName;
        i.tth = tth;
        i.fileSize = fileSize;
        i.fileHost = fileHost;
        downloadTransferQueue.value(priority)->append(i);
        // start the next highest priority queued download, unless max downloads active.
        if (currentDownloadCount < maximumSimultaneousDownloads)
            startNextDownload();
    }
}

// get next item to be downloaded off the queue
DownloadTransferQueueItem TransferManager::getNextQueuedDownload()
{
    QMapIterator<int, QList<DownloadTransferQueueItem>* > i(downloadTransferQueue);
    while (i.hasNext())
    {
        QList<DownloadTransferQueueItem>* l = i.next().value();
        if (!l->isEmpty())
        {
            DownloadTransferQueueItem nextItem;
            nextItem = l->first();
            l->removeFirst();
            return nextItem;
        }
    }
    DownloadTransferQueueItem noItem;
    noItem.filePathName = "";
    return noItem;
}

void TransferManager::startNextDownload()
{
    DownloadTransferQueueItem i = getNextQueuedDownload();
    if (i.filePathName == "")
        return;

    currentDownloadCount++;
    Transfer *t = new DownloadTransfer(this);
    connect(t, SIGNAL(abort(Transfer*)), this, SLOT(destroyTransferObject(Transfer*)));
    connect(t, SIGNAL(hashBucketRequest(QByteArray,int,QByteArray)), this, SIGNAL(hashBucketRequest(QByteArray,int,QByteArray)));
    connect(t, SIGNAL(TTHTreeRequest(QHostAddress,QByteArray,quint32,quint32)),
            this, SIGNAL(TTHTreeRequest(QHostAddress,QByteArray,quint32,quint32)));
    connect(t, SIGNAL(searchTTHAlternateSources(QByteArray)), this, SIGNAL(searchTTHAlternateSources(QByteArray)));
    connect(t, SIGNAL(loadTTHSourcesFromDatabase(QByteArray)), this, SIGNAL(loadTTHSourcesFromDatabase(QByteArray)));
    connect(t, SIGNAL(requestProtocolCapability(QHostAddress,Transfer*)), this, SLOT(requestPeerProtocolCapability(QHostAddress,Transfer*)));
    connect(t, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32)),
            this, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32)));
    connect(t, SIGNAL(flushBucket(QString,QByteArray*)), this, SIGNAL(flushBucket(QString,QByteArray*)));
    connect(t, SIGNAL(assembleOutputFile(QString,QString,int,int)), this, SIGNAL(assembleOutputFile(QString,QString,int,int)));
    connect(t, SIGNAL(flushBucketDirect(QString,int,QByteArray*,QByteArray)), this, SIGNAL(flushBucketDirect(QString,int,QByteArray*,QByteArray)));
    connect(t, SIGNAL(renameIncompleteFile(QString)), this, SIGNAL(renameIncompleteFile(QString)));
    connect(t, SIGNAL(transferFinished(QByteArray)), this, SLOT(transferDownloadCompleted(QByteArray)));
    connect(t, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)), this, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)));
    connect(t, SIGNAL(requestNextSegmentId()), this, SLOT(requestNextSegmentId()));
    connect(this, SIGNAL(setSegmentId(quint32)), t, SLOT(setNextSegmentId(quint32)));
    connect(t, SIGNAL(saveBucketFlushStateBitmap(QByteArray,QByteArray)), this, SIGNAL(saveBucketFlushStateBitmap(QByteArray,QByteArray)));

    t->setFileName(i.filePathName);
    t->setTTH(i.tth);
    t->setFileSize(i.fileSize);
    t->setProtocolOrderPreference(pSettings->value("protocolHint").toAscii());
    t->addPeer(i.fileHost);
    transferObjectTable.insertMulti(i.tth, t);
    emit loadBucketFlushStateBitmap(i.tth);
    emit loadTTHSourcesFromDatabase(i.tth);
    emit searchTTHAlternateSources(i.tth);
    t->startTransfer();

    emit downloadStarted(i.tth);
}

void TransferManager::changeQueuedDownloadPriority(int oldPriority, int newPriority, QByteArray tth)
{
    DownloadTransferQueueItem item;
    int index = 0;
    if (downloadTransferQueue.contains(oldPriority))
    {
        QListIterator<DownloadTransferQueueItem> i(*downloadTransferQueue.value(oldPriority));
        while (i.hasNext())
        {
            if (i.peekNext().tth == tth)
            {
                item = i.next();
                break;
            }
            i.next();
            index++;
        }
    }
    if (item.filePathName != "")
    {
        //Remove from old priority queue
        downloadTransferQueue.value(oldPriority)->removeAt(index);

        //Add to new priority queue
        if (!downloadTransferQueue.contains(newPriority))
        {
            //Add priority type if it doesn't exist
            QList<DownloadTransferQueueItem> *list = new QList<DownloadTransferQueueItem>();
            downloadTransferQueue.insert(newPriority, list);
        }
        if (oldPriority > newPriority)
            downloadTransferQueue.value(newPriority)->append(item);
        else
            downloadTransferQueue.value(newPriority)->prepend(item);
    }
}

// downloads that are still queued can just be lifted off the queue, their transfer objects do not exist yet.
void TransferManager::removeQueuedDownload(int priority, QByteArray tth)
{
    if (downloadTransferQueue.contains(priority))
    {
        int pos = -1;
        int foundPos = -1;
        QListIterator<DownloadTransferQueueItem> i(*downloadTransferQueue.value(priority));
        while (i.hasNext())
        {
            pos++;
            if (i.next().tth == tth)
            {
                foundPos = pos;
                break;
            }
        }
        if (foundPos > -1)
        {   // As far as I understand, a list's index will always start at 0 and follow sequentially. Touch wood.
            downloadTransferQueue.value(priority)->removeAt(foundPos);
        }
    }

    //Stop the transfer if it's not in the queue (it might've already started)
    stopTransfer(tth, TRANSFER_TYPE_DOWNLOAD, QHostAddress());
}

//Stop a transfer already running
void TransferManager::stopTransfer(QByteArray tth, int transferType, QHostAddress hostAddr)
{
    Transfer *t;

    if (transferType == TRANSFER_TYPE_DOWNLOAD)
        t = getTransferObjectPointer(tth, transferType);
    else
        t = getTransferObjectPointer(tth, transferType, &hostAddr);
    if (t)
    {
        //Abort transfer before deletion
        t->abortTransfer();
        //t->deleteLater();
        //transferObjectTable.remove(tth, t);

        //Start next transfer
        if (currentDownloadCount < maximumSimultaneousDownloads)
            startNextDownload();
    }
}

//Download completed
void TransferManager::transferDownloadCompleted(QByteArray tth)
{
    //Update counts and start next download in queue
    if (currentDownloadCount-1 < maximumSimultaneousDownloads)
        startNextDownload();

    //Emit completed to GUI
    emit downloadCompleted(tth);
}

//Get transfer status
void TransferManager::requestGlobalTransferStatus()
{
    emit returnGlobalTransferStatus(getGlobalTransferStatus());
}

// look for pointer to Transfer object matching tth, transfer type and host address
Transfer* TransferManager::getTransferObjectPointer(QByteArray &tth, int transferType, QHostAddress *hostAddr)
{
    if (hostAddr == 0)
        hostAddr = &zeroHostAddress;

    if (transferObjectTable.contains(tth))
    {
        QListIterator<Transfer*> it(transferObjectTable.values(tth));
        while (it.hasNext())
        {
            Transfer* p = it.next();
            //if ((p->getTransferType() == transferType) && (p->getRemoteHost()->toString() == hostAddr.toString()))
            if ((p->getTransferType() == transferType) && (*p->getRemoteHost() == *hostAddr))
                return p;
        }
    }
    return 0;
}

// return a list of structs that contain the status of all the current transfers
QList<TransferItemStatus> TransferManager::getGlobalTransferStatus()
{
    QList<TransferItemStatus> status;
    QHashIterator<QByteArray, Transfer*> it(transferObjectTable);
    while (it.hasNext())
    {
        TransferItemStatus tis;
        tis.TTH = *it.peekNext().value()->getTTH();
        tis.filePathName = *it.peekNext().value()->getFileName();
        tis.transferType = it.peekNext().value()->getTransferType();
        tis.transferStatus = it.peekNext().value()->getTransferStatus();
        tis.transferProgress = it.peekNext().value()->getTransferProgress();
        tis.transferRate = it.peekNext().value()->getTransferRate();
        tis.uptime = it.peekNext().value()->getUptime();
        tis.host = *it.peekNext().value()->getRemoteHost();
        tis.onlineSegments = it.peekNext().value()->getSegmentCount();
        tis.segmentStatuses = it.peekNext().value()->getSegmentStatuses();
        tis.segmentBitmap = it.peekNext().value()->getTransferStateBitmap();
        status.append(tis);
        
        it.next();
    }
    return status;
}

// signals from db restore and incoming tth search both route here
void TransferManager::incomingTTHSource(QByteArray tth, QHostAddress sourcePeer)
{
    if (transferObjectTable.contains(tth))
        transferObjectTable.value(tth)->addPeer(sourcePeer);
}

// packet containing part of a tree
// qint32 bucket number followed by 24-byte QByteArray of bucket 1MBTTH
// therefore, 2.25 petabyte max file size, should suffice.
void TransferManager::incomingTTHTree(QByteArray tth, QByteArray tree)
{
    Transfer *t = getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->TTHTreeReply(tree);
}

void TransferManager::hashBucketReply(QByteArray rootTTH, int bucketNumber, QByteArray bucketTTH)
{
    Transfer *t = getTransferObjectPointer(rootTTH, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->hashBucketReply(bucketNumber, bucketTTH);
    // should be no else, if the download object mysteriously disappeared somewhere, we can just silently drop the message here.
}

void TransferManager::incomingProtocolCapabilityResponse(QHostAddress peer, char protocols)
{
    peerProtocolCapabilities.insert(peer, protocols);
    if (peerProtocolDiscoveryWaitingPool.contains(peer) && peerProtocolDiscoveryWaitingPool.value(peer))
    {
        peerProtocolDiscoveryWaitingPool.value(peer)->receivedPeerProtocolCapability(peer, protocols);
        peerProtocolDiscoveryWaitingPool.take(peer);
    }
}

void TransferManager::requestPeerProtocolCapability(QHostAddress peer, Transfer *transferObject)
{
    //================================================================================================================
    //After a few hours of debugging = This shouldn't be a direct call - it messes up the calling order!
    //It will call getSegmentForDownloading BEFORE startTransfer which will ensure the download is never started since:
    
    //transferSegmentStateBitmap has length 0 before startTransfer
    //which ensures a segment is returned with segmentEnd = 0
    //which makes downloadNextAvailableChunk think the segment is done

    //The first download will work since the call to getSegmentForDownloading is queued. The second, third etc from the same source will fail.
    //Either this function should always use the deferred protocol capability request (through the emit below) or the whole calling order should be reexamined... 
    //(Which by the way is pretty obtuse - probably since this protocol capability functionality was added later. A comment or two on the logical process would've been nice ;) )
    //================================================================================================================

    //if (peerProtocolCapabilities.contains(peer))
    //    transferObject->receivedPeerProtocolCapability(peer, peerProtocolCapabilities.value(peer));
    //else
    //{
        peerProtocolDiscoveryWaitingPool.insert(peer, transferObject);
        emit requestProtocolCapability(peer);
    //}
}

void TransferManager::bucketFlushed(QByteArray tth, int bucketNo)
{
    Transfer* t = getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->bucketFlushed(bucketNo);
    //qDebug() << "TM bucket flush"; << t;
}

void TransferManager::bucketFlushFailed(QByteArray tth, int bucketNo)
{
    Transfer* t = getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->bucketFlushFailed(bucketNo);
    //qDebug() << "TM bucket flush fail" << t;
}

void TransferManager::setMaximumSimultaneousDownloads(int n)
{
    maximumSimultaneousDownloads = n;
}

void TransferManager::setMaximumSimultaneousUploads(int n)
{
    maximumSimultaneousUploads = n;
}

void TransferManager::requestNextSegmentId()
{
    nextSegmentId++;
    if (nextSegmentId == 0)
        nextSegmentId++;

    emit setSegmentId(nextSegmentId);
}

void TransferManager::restoreBucketFlushStateBitmap(QByteArray tth, QByteArray bitmap)
{
    Transfer *t = getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->setBucketFlushStateBitmap(bitmap);
}

TransferSegment* TransferManager::getTransferSegmentPointer(quint32 segmentId)
{
    // QHash automatically returns 0 for TransferSegment* when key not found
    return transferSegmentPointers.value(segmentId);
}

void TransferManager::setTransferSegmentPointer(quint32 segmentId, TransferSegment *segment)
{
    transferSegmentPointers.insert(segmentId, segment);
}

void TransferManager::removeTransferSegmentPointer(quint32 segmentId)
{
    transferSegmentPointers.remove(segmentId);
}
